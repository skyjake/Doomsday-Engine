/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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

/*
 * p_saveg.c: Save Game I/O
 *
 * Compiles for jDoom and jHeretic
 */

#if __JDOOM__ || __WOLFTC__ || __JHERETIC__

// HEADER FILES ------------------------------------------------------------

#include <lzss.h>

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#endif

#include "p_saveg.h"
#include "f_infine.h"
#include "d_net.h"
#include "p_svtexarc.h"
#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_player.h"

// MACROS ------------------------------------------------------------------

#if __DOOM64TC__
# define MY_SAVE_MAGIC         0x1D6420F4
# define MY_CLIENT_SAVE_MAGIC  0x2D6420F4
# define MY_SAVE_VERSION       5
# define SAVESTRINGSIZE        24
# define CONSISTENCY           0x2c
# define SAVEGAMENAME          "D64Sav"
# define CLIENTSAVEGAMENAME    "D64Cl"
# define SAVEGAMEEXTENSION     "6sg"
#elif __WOLFTC__
# define MY_SAVE_MAGIC         0x1A8AFF08
# define MY_CLIENT_SAVE_MAGIC  0x2A8AFF08
# define MY_SAVE_VERSION       5
# define SAVESTRINGSIZE        24
# define CONSISTENCY           0x2c
# define SAVEGAMENAME          "WolfSav"
# define CLIENTSAVEGAMENAME    "WolfCl"
# define SAVEGAMEEXTENSION     "wsg"
#elif __JDOOM__
# define MY_SAVE_MAGIC         0x1DEAD666
# define MY_CLIENT_SAVE_MAGIC  0x2DEAD666
# define MY_SAVE_VERSION       5
# define SAVESTRINGSIZE        24
# define CONSISTENCY           0x2c
# define SAVEGAMENAME          "DoomSav"
# define CLIENTSAVEGAMENAME    "DoomCl"
# define SAVEGAMEEXTENSION     "dsg"
#elif __JHERETIC__
# define MY_SAVE_MAGIC         0x7D9A12C5
# define MY_CLIENT_SAVE_MAGIC  0x1062AF43
# define MY_SAVE_VERSION       5
# define SAVESTRINGSIZE        24
# define CONSISTENCY           0x9d
# define SAVEGAMENAME          "HticSav"
# define CLIENTSAVEGAMENAME    "HticCl"
# define SAVEGAMEEXTENSION     "hsg"
#endif

#define PRE_VER5_END_SPECIALS   7

#define FF_FULLBRIGHT       0x8000 // used to be flag in thing->frame
#define FF_FRAMEMASK        0x7fff

// TYPES -------------------------------------------------------------------

typedef struct saveheader_s {
    int     magic;
    int     version;
    int     gamemode;
    char    description[SAVESTRINGSIZE];
    byte    skill;
    byte    episode;
    byte    map;
    byte    deathmatch;
    byte    nomonsters;
    byte    respawn;
    int     leveltime;
    byte    players[MAXPLAYERS];
    unsigned int gameid;
} saveheader_t;

typedef struct playerheader_s {
    int     numpowers;
    int     numkeys;
    int     numfrags;
    int     numweapons;
    int     numammotypes;
    int     numpsprites;
#if __DOOM64TC__
    int     numartifacts;
#endif
} playerheader_t;

// Thinker Save flags
#define TSF_SERVERONLY      0x01    // Only saved by servers.
#define TSF_SPECIAL         0x02    // Its a special (eg T_MoveCeiling)

typedef struct thinkerinfo_s {
    thinkerclass_t  thinkclass;
    think_t         function;
    int             flags;
    void          (*Write) ();
    int           (*Read) ();
    size_t          size;
} thinkerinfo_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void SV_WriteMobj(mobj_t *mobj);
static int  SV_ReadMobj(thinker_t *th);
static void SV_WriteCeiling(ceiling_t* ceiling);
static int  SV_ReadCeiling(ceiling_t* ceiling);
static void SV_WriteDoor(vldoor_t* door);
static int  SV_ReadDoor(vldoor_t* door);
static void SV_WriteFloor(floormove_t* floor);
static int  SV_ReadFloor(floormove_t* floor);
static void SV_WritePlat(plat_t* plat);
static int  SV_ReadPlat(plat_t* plat);
static void SV_WriteFlash(lightflash_t* flash);
static int  SV_ReadFlash(lightflash_t* flash);
static void SV_WriteStrobe(strobe_t* strobe);
static int  SV_ReadStrobe(strobe_t* strobe);
static void SV_WriteGlow(glow_t* glow);
static int  SV_ReadGlow(glow_t* glow);
#if __JDOOM__
static void SV_WriteFlicker(fireflicker_t* flicker);
static int  SV_ReadFlicker(fireflicker_t* flicker);
#endif

#if __DOOM64TC__
static void SV_WriteBlink(lightblink_t* flicker);
static int  SV_ReadBlink(lightblink_t* flicker);
#endif

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

LZFILE *savefile;
char    save_path[128];         /* = "savegame\\"; */
char    client_save_path[128];  /* = "savegame\\client\\"; */

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static saveheader_t hdr;
static mobj_t **thing_archive;
static int thing_archiveSize;
static int SaveToRealPlayer[MAXPLAYERS];

static int numSoundTargets = 0;

static byte *junkbuffer;        // Old save data is read into here.

static playerheader_t playerHeader;

/*
 * IMPORTANT:
 * Add new thinkers to the end. Existing indices must remain the same.
 */
static thinkerinfo_t thinkerInfo[] = {
    {
      tc_end,
      NULL,
      0,
      NULL,
      NULL,
      0
    },
    {
      tc_mobj,
      P_MobjThinker,
      TSF_SERVERONLY,
      SV_WriteMobj,
      SV_ReadMobj,
      sizeof(mobj_t)
    },
    {
      tc_xgmover,
      XS_PlaneMover,
      0,
      SV_WriteXGPlaneMover,
      SV_ReadXGPlaneMover,
      sizeof(xgplanemover_t)
    },
    {
      tc_ceiling,
      T_MoveCeiling,
      TSF_SPECIAL,
      SV_WriteCeiling,
      SV_ReadCeiling,
      sizeof(ceiling_t)
    },
    {
      tc_door,
      T_VerticalDoor,
      TSF_SPECIAL,
      SV_WriteDoor,
      SV_ReadDoor,
      sizeof(vldoor_t)
    },
    {
      tc_floor,
      T_MoveFloor,
      TSF_SPECIAL,
      SV_WriteFloor,
      SV_ReadFloor,
      sizeof(floormove_t)
    },
    {
      tc_plat,
      T_PlatRaise,
      TSF_SPECIAL,
      SV_WritePlat,
      SV_ReadPlat,
      sizeof(plat_t)
    },
    {
      tc_flash,
      T_LightFlash,
      TSF_SPECIAL,
      SV_WriteFlash,
      SV_ReadFlash,
      sizeof(lightflash_t)
    },
    {
      tc_strobe,
      T_StrobeFlash,
      TSF_SPECIAL,
      SV_WriteStrobe,
      SV_ReadStrobe,
      sizeof(strobe_t)
    },
    {
      tc_glow,
      T_Glow,
      TSF_SPECIAL,
      SV_WriteGlow,
      SV_ReadGlow,
      sizeof(glow_t)
    },
#if __JDOOM__
    {
      tc_flicker,
      T_FireFlicker,
      TSF_SPECIAL,
      SV_WriteFlicker,
      SV_ReadFlicker,
      sizeof(fireflicker_t)
    },
#endif
#if __DOOM64TC__
    {
      tc_blink,
      T_LightBlink,
      TSF_SPECIAL,
      SV_WriteBlink,
      SV_ReadBlink,
      sizeof(lightblink_t)
    },
#endif
    // Terminator
    { tc_null, NULL, 0, NULL, NULL, 0 }
};

// CODE --------------------------------------------------------------------

/*
 * Return a ptr to the thinkerinfo for the given thinker.
 */
static thinkerinfo_t* ThinkerInfo(thinker_t *th)
{
    thinkerinfo_t *thInfo = thinkerInfo;

    if(!th)
        return NULL;

    while(thInfo->thinkclass != tc_null)
    {
        if(thInfo->function == th->function)
            return thInfo;

        thInfo++;
    }

    return NULL;
}

/*
 * Must be called before saving or loading any data.
 */
static void SV_InitThingArchive(boolean load)
{
    int count = 0;

    if(load)
    {
        if(hdr.version >= 5)
            count = SV_ReadLong();
        else
            count = 1024; // Limit in previous versions.
    }
    else
    {
        thinker_t *th;

        // Count the number of mobjs we'll be writting.
        th = thinkercap.next;
        while(th != &thinkercap)
        {
            if(th->function == P_MobjThinker)
                count++;

            th = th->next;
        }

        SV_WriteLong(count);
    }

    thing_archive = calloc(count, sizeof(mobj_t*));
    thing_archiveSize = count;
}

/*
 * Free the thing archive. Called when load is complete.
 */
static void SV_FreeThingArchive(void)
{
    free(thing_archive);
    thing_archive = NULL;
    thing_archiveSize = 0;
}

/*
 * Called by the write code to get archive numbers.
 * If the mobj is already archived, the existing number is returned.
 * Number zero is not used.
 */
unsigned short SV_ThingArchiveNum(mobj_t *mo)
{
    int     i;
    int     first_empty = -1;

    // We only archive valid mobj thinkers.
    if(mo == NULL || ((thinker_t *) mo)->function == NOPFUNC)
        return 0;

    for(i = 0; i < thing_archiveSize; i++)
    {
        if(!thing_archive[i] && first_empty < 0)
        {
            first_empty = i;
            continue;
        }
        if(thing_archive[i] == mo)
            return i + 1;
    }

    if(first_empty == -1)
    {
        Con_Error("SV_ThingArchiveNum: Thing archive exhausted!\n");
        return 0;               // No number available!
    }

    // OK, place it in an empty pos.
    *(thing_archive + first_empty) = mo;
    return first_empty + 1;
}

/*
 * Used by the read code when mobjs are read.
 */
static void SV_SetArchiveThing(mobj_t *mo, int num)
{
    if(!num)
        return;
    *(thing_archive + num - 1) = mo;
}

mobj_t *SV_GetArchiveThing(int num)
{
    if(num == 0)
        return NULL;

    if(num < 0)
        Con_Message("SV_GetArchiveThing: Invalid NUM %i??\n", num);

    return *(thing_archive + num - 1);
}

unsigned int SV_GameID(void)
{
    return Sys_GetRealTime() + (leveltime << 24);
}

void SV_Write(void *data, int len)
{
    lzWrite(data, len, savefile);
}

void SV_WriteByte(byte val)
{
    lzPutC(val, savefile);
}

void SV_WriteShort(short val)
{
    lzPutW(val, savefile);
}

void SV_WriteLong(long val)
{
    lzPutL(val, savefile);
}

void SV_WriteFloat(float val)
{
    long temp = 0;
    memcpy(&temp, &val, 4);
    lzPutL(temp, savefile);
}

void SV_Read(void *data, int len)
{
    lzRead(data, len, savefile);
}

byte SV_ReadByte()
{
    return lzGetC(savefile);
}

short SV_ReadShort()
{
    return lzGetW(savefile);
}

long SV_ReadLong()
{
    return lzGetL(savefile);
}

float SV_ReadFloat()
{
    long    val = lzGetL(savefile);
    float   returnValue = 0;

    memcpy(&returnValue, &val, 4);
    return returnValue;
}

static void SV_WritePlayer(int playernum)
{
    player_t temp, *pl = &temp;
    ddplayer_t *dpl;
    int     i;

    memcpy(pl, players + playernum, sizeof(temp));
    dpl = pl->plr;
    for(i = 0; i < playerHeader.numpsprites; i++)
    {
        if(temp.psprites[i].state)
        {
            temp.psprites[i].state =
                (state_t *) (temp.psprites[i].state - states);
        }
    }

    SV_WriteByte(3);            // Write a version byte.
    SV_WriteLong(pl->playerstate);
    SV_WriteLong(dpl->viewz);
    SV_WriteLong(dpl->viewheight);
    SV_WriteLong(dpl->deltaviewheight);
    SV_WriteFloat(dpl->lookdir);
    SV_WriteLong(pl->bob);

    SV_WriteLong(pl->health);
    SV_WriteLong(pl->armorpoints);
    SV_WriteLong(pl->armortype);

#if __DOOM64TC__
    SV_WriteLong(pl->laserpw); // d64tc
    SV_WriteLong(pl->lasericon1); // added in outcast
    SV_WriteLong(pl->lasericon2); // added in outcast
    SV_WriteLong(pl->lasericon3); // added in outcast
    SV_WriteLong(pl->outcastcycle); // added in outcast
    SV_WriteLong(pl->helltime); // added in outcast
    SV_WriteLong(pl->devicetime); // added in outcast
#endif

    SV_Write(pl->powers, playerHeader.numpowers * 4);
#if __DOOM64TC__
    //SV_WriteLong(cheatenable); // added in outcast
#endif
    SV_Write(pl->keys, playerHeader.numkeys * 4);
#if __DOOM64TC__
    SV_Write(pl->artifacts, playerHeader.numartifacts * 4);
#endif
    SV_WriteLong(pl->backpack);

    SV_Write(pl->frags, playerHeader.numfrags * 4);
    SV_WriteLong(pl->readyweapon);
    SV_WriteLong(pl->pendingweapon);

    SV_Write(pl->weaponowned, playerHeader.numweapons * 4);
    SV_Write(pl->ammo, playerHeader.numammotypes * 4);
    SV_Write(pl->maxammo, playerHeader.numammotypes * 4);

    SV_WriteLong(pl->attackdown);
    SV_WriteLong(pl->usedown);

    SV_WriteLong(pl->cheats);

    SV_WriteLong(pl->refire);

    SV_WriteLong(pl->killcount);
    SV_WriteLong(pl->itemcount);
    SV_WriteLong(pl->secretcount);

    SV_WriteLong(pl->damagecount);
    SV_WriteLong(pl->bonuscount);

    SV_WriteLong(dpl->extralight);
    SV_WriteLong(dpl->fixedcolormap);
    SV_WriteLong(pl->colormap);
    SV_Write(pl->psprites, playerHeader.numpsprites * sizeof(pspdef_t));

    SV_WriteLong(pl->didsecret);

    // Added in ver 2 with __JDOOM__
    SV_WriteLong(pl->flyheight);
#ifdef __JHERETIC__
    SV_Write(pl->inventory, 4 * 2 * 14);
    SV_WriteLong(pl->readyArtifact);
    SV_WriteLong(pl->artifactCount);
    SV_WriteLong(pl->inventorySlotNum);
    SV_WriteLong(pl->chickenPeck);
    SV_WriteLong(pl->morphTics);
    SV_WriteLong(pl->flamecount);

    // Added in ver 2
    SV_WriteByte(pl->class);
#endif
}

static void SV_ReadPlayer(player_t *pl)
{
    ddplayer_t *dpl = pl->plr;
    int     j;
    byte ver;

    ver = SV_ReadByte();

    pl->playerstate = SV_ReadLong();
    dpl->viewz = SV_ReadLong();
    dpl->viewheight = SV_ReadLong();
    dpl->deltaviewheight = SV_ReadLong();
    dpl->lookdir = SV_ReadFloat();
    pl->bob = SV_ReadLong();

    pl->health = SV_ReadLong();
    pl->armorpoints = SV_ReadLong();
    pl->armortype = SV_ReadLong();

#if __DOOM64TC__
    pl->laserpw = SV_ReadLong(); // d64tc
    pl->lasericon1 = SV_ReadLong(); // added in outcast
    pl->lasericon2 = SV_ReadLong(); // added in outcast
    pl->lasericon3 = SV_ReadLong(); // added in outcast
    pl->outcastcycle = SV_ReadLong(); // added in outcast
    pl->helltime = SV_ReadLong(); // added in outcast
    pl->devicetime = SV_ReadLong(); // added in outcast
#endif
    SV_Read(pl->powers, playerHeader.numpowers * 4);
#if __DOOM64TC__
    //cheatenable = SV_ReadLong(); // added in outcast
#endif
    SV_Read(pl->keys, playerHeader.numkeys * 4);
#if __DOOM64TC__
    SV_Read(pl->artifacts, playerHeader.numartifacts * 4);
#endif
    pl->backpack = SV_ReadLong();

    SV_Read(pl->frags, playerHeader.numfrags * 4);
    pl->readyweapon = SV_ReadLong();
    pl->pendingweapon = SV_ReadLong();

    SV_Read(pl->weaponowned, playerHeader.numweapons * 4);
    SV_Read(pl->ammo, playerHeader.numammotypes * 4);
    SV_Read(pl->maxammo, playerHeader.numammotypes * 4);

    pl->attackdown = SV_ReadLong();
    pl->usedown = SV_ReadLong();

    pl->cheats = SV_ReadLong();

    pl->refire = SV_ReadLong();

    pl->killcount = SV_ReadLong();
    pl->itemcount = SV_ReadLong();
    pl->secretcount = SV_ReadLong();

    pl->damagecount = SV_ReadLong();
    pl->bonuscount = SV_ReadLong();

    dpl->extralight = SV_ReadLong();
    dpl->fixedcolormap = SV_ReadLong();
    pl->colormap = SV_ReadLong();
    SV_Read(pl->psprites, playerHeader.numpsprites * sizeof(pspdef_t));

    pl->didsecret = SV_ReadLong();

#ifdef __JDOOM__
    if(ver == 2) // nolonger used in >= ver 3
        /*pl->messageTics =*/ SV_ReadLong();

    if(ver >= 2)
        pl->flyheight = SV_ReadLong();
#endif

#ifdef __JHERETIC__
    if(ver < 3) // nolonger used in >= ver 3
        /*pl->messageTics =*/ SV_ReadLong();

    pl->flyheight = SV_ReadLong();
    SV_Read(pl->inventory, 4 * 2 * 14);
    pl->readyArtifact = SV_ReadLong();
    pl->artifactCount = SV_ReadLong();
    pl->inventorySlotNum = SV_ReadLong();
    pl->chickenPeck = SV_ReadLong();
    pl->morphTics = SV_ReadLong();
    pl->flamecount = SV_ReadLong();

    if(ver >= 2)
        pl->class = SV_ReadByte();
#endif

    for(j = 0; j < playerHeader.numpsprites; j++)
        if(pl->psprites[j].state)
        {
            pl->psprites[j].state = &states[(int) pl->psprites[j].state];
        }

    // Mark the player for fixpos and fixangles.
    dpl->flags |= DDPF_FIXPOS | DDPF_FIXANGLES | DDPF_FIXMOM;
    pl->update = PSF_REBORN;
}

#define MOBJ_SAVEVERSION 7
static void SV_WriteMobj(mobj_t *mobj)
{
    mobj_t  mo;

    SV_WriteByte(tc_mobj);

    // Mangle it!
    memcpy(&mo, mobj, sizeof(mo));
    mo.state = (state_t *) (mo.state - states);
    if(mo.player)
        mo.player = (player_t *) ((mo.player - players) + 1);

    // Version.
    // 4: Added byte 'translucency'
    // 5: Added byte 'vistarget'
    // 5: Added tracer in jDoom
    // 5: Added dropoff fix in jHeretic
    // 5: Added long 'floorclip'
    // 6: Added proper respawn data
    // 6: Added flags 2 in jDoom
    // 6: Added damage
    // 7: Added generator in jHeretic
    SV_WriteByte(MOBJ_SAVEVERSION);

    // A version 2 features: archive number and target.
    SV_WriteShort(SV_ThingArchiveNum(mobj));
    SV_WriteShort(SV_ThingArchiveNum(mo.target));

#if __JDOOM__
    // Ver 5 features: Save tracer (fixes Archvile, Revenant bug)
    SV_WriteShort(SV_ThingArchiveNum(mo.tracer));
#endif

    SV_WriteShort(SV_ThingArchiveNum(mo.onmobj));

    // Info for drawing: position.
    SV_WriteLong(mo.pos[VX]);
    SV_WriteLong(mo.pos[VY]);
    SV_WriteLong(mo.pos[VZ]);

    //More drawing info: to determine current sprite.
    SV_WriteLong(mo.angle);     // orientation
    SV_WriteLong(mo.sprite);    // used to find patch_t and flip value
    SV_WriteLong(mo.frame);

    // The closest interval over all contacted Sectors.
    SV_WriteLong(mo.floorz);
    SV_WriteLong(mo.ceilingz);

    // For movement checking.
    SV_WriteLong(mo.radius);
    SV_WriteLong(mo.height);

    // Momentums, used to update position.
    SV_WriteLong(mo.momx);
    SV_WriteLong(mo.momy);
    SV_WriteLong(mo.momz);

    // If == validcount, already checked.
    SV_WriteLong(mo.valid);

    SV_WriteLong(mo.type);

    SV_WriteLong(mo.tics);      // state tic counter
    SV_WriteLong((int) mo.state);
    SV_WriteLong(mo.flags);
    SV_WriteLong(mo.health);

    // Movement direction, movement generation (zig-zagging).
    SV_WriteLong(mo.movedir);   // 0-7
    SV_WriteLong(mo.movecount); // when 0, select a new dir

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    SV_WriteLong(mo.reactiontime);

#if __DOOM64TC__
    SV_WriteLong(mo.floatswitch); // added in outcast
    SV_WriteLong(mo.floattics); // added in outcast
#endif

    // If >0, the target will be chased
    // no matter what (even if shot)
    SV_WriteLong(mo.threshold);

    // Additional info record for player avatars only.
    // Only valid if type == MT_PLAYER
    SV_WriteLong((int) mo.player);

    // Player number last looked for.
    SV_WriteLong(mo.lastlook);

    // For nightmare/multiplayer respawn.
    SV_WriteLong(mo.spawninfo.pos[VX]);
    SV_WriteLong(mo.spawninfo.pos[VY]);
    SV_WriteLong(mo.spawninfo.pos[VZ]);
    SV_WriteLong(mo.spawninfo.angle);
    SV_WriteLong(mo.spawninfo.type);
    SV_WriteLong(mo.spawninfo.options);

    SV_WriteLong(mo.intflags);  // killough $dropoff_fix: internal flags
    SV_WriteLong(mo.dropoffz);  // killough $dropoff_fix
    SV_WriteLong(mo.gear);      // killough used in torque simulation

    SV_WriteLong(mo.damage);
    SV_WriteLong(mo.flags2);
#ifdef __JHERETIC__
    SV_WriteLong(mo.special1);
    SV_WriteLong(mo.special2);
#endif

    SV_WriteByte(mo.translucency);
    SV_WriteByte((byte)(mo.vistarget +1));

    SV_WriteLong(mo.floorclip);

#ifdef __JHERETIC__
    // Ver 7 features: generator
    SV_WriteShort(SV_ThingArchiveNum(mo.generator));
#endif
}

/*
 * Fixes the mobj flags in older save games to the current values.
 *
 * Called after loading a save game where the mobj format is older than
 * the current version.
 *
 * @param   mo          Ptr to the mobj whoose flags are to be updated.
 * @param   ver         The MOBJ save version to update from.
 */
void SV_UpdateReadMobjFlags(mobj_t *mo, int ver)
{
    if(ver < 6)
    {
        // mobj.flags
#if __JDOOM__
        // switched values for MF_BRIGHTSHADOW <> MF_BRIGHTEXPLODE
        if((mo->flags & MF_BRIGHTEXPLODE) != (mo->flags & MF_BRIGHTSHADOW))
        {

            if(mo->flags & MF_BRIGHTEXPLODE) // previously MF_BRIGHTSHADOW
            {
                mo->flags |= MF_BRIGHTSHADOW;
                mo->flags &= ~MF_BRIGHTEXPLODE;
            }
            else // previously MF_BRIGHTEXPLODE
            {
                mo->flags |= MF_BRIGHTEXPLODE;
                mo->flags &= ~MF_BRIGHTSHADOW;
            }
        } // else they were both on or off so it doesn't matter.
#endif
        // Remove obsoleted flags in earlier save versions.
        mo->flags &= ~MF_V6OBSOLETE;

        // mobj.flags2
#if __JDOOM__
        // jDoom only gained flags2 in ver 6 so all we can do is to
        // apply the values as set in the mobjinfo.
        // Non-persistent flags might screw things up a lot worse otherwise.
        mo->flags2 = mo->info->flags2;
#endif
    }
}

static int SV_ReadMobj(thinker_t *th)
{
    mobj_t *mo = (mobj_t*) th;
    int     ver = SV_ReadByte();

    if(ver >= 2)
    {
        // Version 2 has mobj archive numbers.
        SV_SetArchiveThing(mo, SV_ReadShort());
        // The reference will be updated after all mobjs are loaded.
        mo->target = (mobj_t *) (int) SV_ReadShort();
    }
    else
        mo->target = NULL;

    // Ver 5 features:
    if(ver >= 5)
    {
#if __JDOOM__
        // Tracer for enemy attacks (updated after all mobjs are loaded).
        mo->tracer = (mobj_t *) (int) SV_ReadShort();
#endif
        // mobj this one is on top of (updated after all mobjs are loaded).
        mo->onmobj = (mobj_t *) (int) SV_ReadShort();
    }
    else
    {
#if __JDOOM__
        mo->tracer = NULL;
#endif
        mo->onmobj = NULL;
    }

    // Info for drawing: position.
    mo->pos[VX] = SV_ReadLong();
    mo->pos[VY] = SV_ReadLong();
    mo->pos[VZ] = SV_ReadLong();

    //More drawing info: to determine current sprite.
    mo->angle = SV_ReadLong();  // orientation
    mo->sprite = SV_ReadLong(); // used to find patch_t and flip value
    mo->frame = SV_ReadLong();  // might be ORed with FF_FULLBRIGHT
    if(mo->frame & FF_FULLBRIGHT)
        mo->frame &= FF_FRAMEMASK; // not used anymore.

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
    mo->info = &mobjinfo[mo->type];

    mo->tics = SV_ReadLong();   // state tic counter
    mo->state = (state_t *) SV_ReadLong();
    mo->flags = SV_ReadLong();

    mo->health = SV_ReadLong();

    // Movement direction, movement generation (zig-zagging).
    mo->movedir = SV_ReadLong();    // 0-7
    mo->movecount = SV_ReadLong();  // when 0, select a new dir

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    mo->reactiontime = SV_ReadLong();

#if __DOOM64TC__
    mo->floatswitch = SV_ReadLong(); // added in outcast
    mo->floattics = SV_ReadLong(); // added in outcast
#endif

    // If >0, the target will be chased
    // no matter what (even if shot)
    mo->threshold = SV_ReadLong();

    // Additional info record for player avatars only.
    // Only valid if type == MT_PLAYER
    mo->player = (player_t *) SV_ReadLong();

    // Player number last looked for.
    mo->lastlook = SV_ReadLong();

    // For nightmare respawn.
    if(ver >= 6)
    {
        mo->spawninfo.pos[VX] = SV_ReadLong();
        mo->spawninfo.pos[VY] = SV_ReadLong();
        mo->spawninfo.pos[VZ] = SV_ReadLong();
        mo->spawninfo.angle = SV_ReadLong();
        mo->spawninfo.type = SV_ReadLong();
        mo->spawninfo.options = SV_ReadLong();
    }
    else
    {
        mo->spawninfo.pos[VX] = (fixed_t) (SV_ReadShort() << FRACBITS);
        mo->spawninfo.pos[VY] = (fixed_t) (SV_ReadShort() << FRACBITS);
        mo->spawninfo.pos[VZ] = ONFLOORZ;
        mo->spawninfo.angle = (angle_t) (ANG45 * (SV_ReadShort() / 45));
        mo->spawninfo.type = (int) SV_ReadShort();
        mo->spawninfo.options = (int) SV_ReadShort();
    }

#ifdef __JDOOM__
    if(ver >= 3)
    {
        mo->intflags = SV_ReadLong();   // killough $dropoff_fix: internal flags
        mo->dropoffz = SV_ReadLong();   // killough $dropoff_fix
        mo->gear = SV_ReadLong();   // killough used in torque simulation
    }
#endif

#ifdef __JHERETIC__
    if(ver >= 5)
    {
        mo->intflags = SV_ReadLong();   // killough $dropoff_fix: internal flags
        mo->dropoffz = SV_ReadLong();   // killough $dropoff_fix
        mo->gear = SV_ReadLong();   // killough used in torque simulation
    }
#endif

#if __JDOOM__
    if(ver >= 6)
    {
        mo->damage = SV_ReadLong();
        mo->flags2 = SV_ReadLong();
    }// Else flags2 will be applied from the defs.
    else
        mo->damage = DDMAXINT; // Use the value set in mo->info->damage

#elif __JHERETIC__
    mo->damage = SV_ReadLong();
    mo->flags2 = SV_ReadLong();
#endif

#if __JHERETIC__
    mo->special1 = SV_ReadLong();
    mo->special2 = SV_ReadLong();
#endif

    // Version 4 has the translucency byte.
    if(ver >= 4)
    {
        mo->translucency = SV_ReadByte();
    }

    // Ver 5 has the vistarget byte and the floorclip long
    if(ver >= 5)
    {
        mo->vistarget = ((short)SV_ReadByte())-1;
        mo->floorclip = SV_ReadLong();
    }

#if __JHERETIC__
    if(ver >= 7)
        mo->generator = (mobj_t *) (int) SV_ReadShort();
    else
        mo->generator = NULL;
#endif

    // Restore! (unmangle)
    mo->state = &states[(int) mo->state];
    if(mo->player)
    {
        // Saved players might be in a different order than at the moment.
        int     pnum = SaveToRealPlayer[(int) mo->player - 1];

        mo->player = &players[pnum];
        mo->dplayer = mo->player->plr;
        mo->dplayer->mo = mo;
        //mo->dplayer->clAngle = mo->angle; /* $unifiedangles */
        mo->dplayer->lookdir = 0; /* $unifiedangles */
    }

    if(mo->dplayer && !mo->dplayer->ingame)
    {
        if(mo->dplayer)
            mo->dplayer->mo = NULL;
        Z_Free(mo);

        return false; // Don't add this thinker.
    }

    mo->visangle = mo->angle >> 16;

    mo->thinker.function = P_MobjThinker;

    // Do we need to update this mobj's flag values?
    if(ver < MOBJ_SAVEVERSION)
        SV_UpdateReadMobjFlags(mo, ver);

    P_SetThingPosition(mo);
    mo->info = &mobjinfo[mo->type];
    mo->floorz = P_GetFixedp(mo->subsector,
                               DMU_SECTOR_OF_SUBSECTOR | DMU_FLOOR_HEIGHT);

    mo->ceilingz = P_GetFixedp(mo->subsector,
                                 DMU_SECTOR_OF_SUBSECTOR | DMU_CEILING_HEIGHT);

    return true; // Add this thinker.
}

static void SV_WriteSector(sector_t *sec)
{
    int     type;
    xsector_t *xsec = P_XSector(sec);

    int     floorheight = P_GetFixedp(sec, DMU_FLOOR_HEIGHT);
    int     ceilingheight = P_GetFixedp(sec, DMU_CEILING_HEIGHT);
    int     floorpic = P_GetIntp(sec, DMU_FLOOR_TEXTURE);
    int     ceilingpic = P_GetIntp(sec, DMU_CEILING_TEXTURE);
    byte    lightlevel = P_GetIntp(sec, DMU_LIGHT_LEVEL);
    float   flooroffx = P_GetFloatp(sec, DMU_FLOOR_OFFSET_X);
    float   flooroffy = P_GetFloatp(sec, DMU_FLOOR_OFFSET_Y);
    float   ceiloffx = P_GetFloatp(sec, DMU_CEILING_OFFSET_X);
    float   ceiloffy = P_GetFloatp(sec, DMU_CEILING_OFFSET_Y);
    byte    rgb[3];

    // Determine type.
    if(xsec->xg)
        type = sc_xg1;
    else if(flooroffx || flooroffy || ceiloffx || ceiloffy)
        type = sc_ploff;
    else
        type = sc_normal;

    // Type byte.
    SV_WriteByte(type);

    SV_WriteByte(1); // Write a version byte.

    SV_WriteShort(floorheight >> FRACBITS);
    SV_WriteShort(ceilingheight >> FRACBITS);
    SV_WriteShort(SV_FlatArchiveNum(floorpic));
    SV_WriteShort(SV_FlatArchiveNum(ceilingpic));
    SV_WriteByte(lightlevel);

    P_GetBytepv(sec, DMU_COLOR, rgb);
    SV_Write(rgb, 3);

    P_GetBytepv(sec, DMU_FLOOR_COLOR, rgb);
    SV_Write(rgb, 3);

    P_GetBytepv(sec, DMU_CEILING_COLOR, rgb);
    SV_Write(rgb, 3);

    SV_WriteShort(xsec->special);
    SV_WriteShort(xsec->tag);

    if(type == sc_xg1 || type == sc_ploff)
    {
        SV_WriteFloat(flooroffx);
        SV_WriteFloat(flooroffy);
        SV_WriteFloat(ceiloffx);
        SV_WriteFloat(ceiloffy);
    }

    if(xsec->xg)                 // Extended General?
    {
        SV_WriteXGSector(sec);
    }

    // Count the number of sound targets
    if(xsec->soundtarget)
        numSoundTargets++;
}

/*
 * Reads all versions of archived sectors.
 * Including the old Ver1.
 */
static void SV_ReadSector(sector_t *sec)
{
    int     ver;
    int     type = 0;
    int     floorTexID;
    int     ceilingTexID;
    byte    rgb[3];
    xsector_t *xsec = P_XSector(sec);

    // Save versions > 1 include a type byte
    if(hdr.version > 1)
        type = SV_ReadByte();

    // Save version > 4 include a version byte
    if(hdr.version > 4)
        ver = SV_ReadByte();

    P_SetFixedp(sec, DMU_FLOOR_HEIGHT, SV_ReadShort() << FRACBITS);
    P_SetFixedp(sec, DMU_CEILING_HEIGHT, SV_ReadShort() << FRACBITS);

    floorTexID = SV_ReadShort();
    ceilingTexID = SV_ReadShort();

    if(hdr.version == 1)
    {
        // In Ver1 the flat numbers are the actual lump numbers.
        int     firstflat = W_CheckNumForName("F_START") + 1;

        floorTexID += firstflat;
        ceilingTexID += firstflat;
    }
    else if(hdr.version >= 4)
    {
        // In Versions >= 4: The flat numbers are actually archive numbers.
        floorTexID = SV_GetArchiveFlat(floorTexID);
        ceilingTexID = SV_GetArchiveFlat(ceilingTexID);
    }

    P_SetIntp(sec, DMU_FLOOR_TEXTURE, floorTexID);
    P_SetIntp(sec, DMU_CEILING_TEXTURE, ceilingTexID);

    // In Ver1 the light level is a short
    if(hdr.version == 1)
        P_SetIntp(sec, DMU_LIGHT_LEVEL, SV_ReadShort());
    else
    {
        P_SetIntp(sec, DMU_LIGHT_LEVEL, SV_ReadByte());

        // Versions > 1 include sector colours
        SV_Read(rgb, 3);
        P_SetBytepv(sec, DMU_COLOR, rgb);
    }

    // Ver 5 includes surface colours
    if(hdr.version >= 5)
    {
        SV_Read(rgb, 3);
        P_SetBytepv(sec, DMU_FLOOR_COLOR, rgb);

        SV_Read(rgb, 3);
        P_SetBytepv(sec, DMU_CEILING_COLOR, rgb);
    }

    xsec->special = SV_ReadShort();
    xsec->tag = SV_ReadShort();

    // Save versions > 1 include a type byte
    if(hdr.version > 1)
    {
        if(type == sc_xg1 || type == sc_ploff)
        {
            P_SetFloatp(sec, DMU_FLOOR_OFFSET_X, SV_ReadFloat());
            P_SetFloatp(sec, DMU_FLOOR_OFFSET_Y, SV_ReadFloat());
            P_SetFloatp(sec, DMU_CEILING_OFFSET_X, SV_ReadFloat());
            P_SetFloatp(sec, DMU_CEILING_OFFSET_Y, SV_ReadFloat());
        }

        if(type == sc_xg1)
            SV_ReadXGSector(sec);
    }
    else
    {
        xsec->specialdata = 0;
    }

    // We'll restore the sound targets latter on
    xsec->soundtarget = 0;
}

static void SV_WriteLine(line_t *li)
{
    int     i;
    int     texid;
    byte    rgb[3];
    byte    rgba[4];

    lineclass_t type;
    xline_t *xli = P_XLine(li);
    side_t *side;

    if(xli->xg)
        type =  lc_xg1;
    else
        type = lc_normal;

    SV_WriteByte(type);
    SV_WriteByte(2); // Write a version byte

    SV_WriteShort(P_GetIntp(li, DMU_FLAGS));
    SV_WriteShort(xli->special);
    SV_WriteShort(xli->tag);

    // For each side
    for(i = 0; i < 2; i++)
    {
        if(i == 0)
            side = P_GetPtrp(li, DMU_SIDE0);
        else
            side = P_GetPtrp(li, DMU_SIDE1);

        if(!side)
            continue;

        SV_WriteShort(P_GetFixedp(side, DMU_TOP_TEXTURE_OFFSET_X) >> FRACBITS);
        SV_WriteShort(P_GetFixedp(side, DMU_TOP_TEXTURE_OFFSET_Y) >> FRACBITS);
        SV_WriteShort(P_GetFixedp(side, DMU_MIDDLE_TEXTURE_OFFSET_X) >> FRACBITS);
        SV_WriteShort(P_GetFixedp(side, DMU_MIDDLE_TEXTURE_OFFSET_Y) >> FRACBITS);
        SV_WriteShort(P_GetFixedp(side, DMU_BOTTOM_TEXTURE_OFFSET_X) >> FRACBITS);
        SV_WriteShort(P_GetFixedp(side, DMU_BOTTOM_TEXTURE_OFFSET_Y) >> FRACBITS);

        texid = P_GetIntp(side, DMU_TOP_TEXTURE);
        SV_WriteShort(SV_TextureArchiveNum(texid));

        texid = P_GetIntp(side, DMU_BOTTOM_TEXTURE);
        SV_WriteShort(SV_TextureArchiveNum(texid));

        texid = P_GetIntp(side, DMU_MIDDLE_TEXTURE);
        SV_WriteShort(SV_TextureArchiveNum(texid));

        P_GetBytepv(side, DMU_TOP_COLOR, rgb);
        SV_Write(rgb, 3);

        P_GetBytepv(side, DMU_BOTTOM_COLOR, rgb);
        SV_Write(rgb, 3);

        P_GetBytepv(side, DMU_MIDDLE_COLOR, rgba);
        SV_Write(rgba, 4);

        SV_WriteLong(P_GetIntp(side, DMU_MIDDLE_BLENDMODE));

        SV_WriteShort(P_GetIntp(side, DMU_FLAGS));
    }

    // Extended General?
    if(xli->xg)
    {
        SV_WriteXGLine(li);
    }
}

/*
 * Reads all versions of archived lines.
 * Including the old Ver1.
 */
static void SV_ReadLine(line_t *li)
{
    enum lineclass_e type = 0;

    int     i;
    int     ver;
    int     topTexID;
    int     bottomTexID;
    int     middleTexID;
    byte    rgb[3];
    byte    rgba[4];
    side_t *side;
    xline_t *xli = P_XLine(li);

    // Save versions > 1 include a type byte
    if(hdr.version > 1)
        type = SV_ReadByte();

    // Save versions > 4 include a version byte
    if(hdr.version > 4)
        ver = SV_ReadByte();

    P_SetIntp(li, DMU_FLAGS, SV_ReadShort());
    xli->special = SV_ReadShort();
    xli->tag = SV_ReadShort();

    // For each side
    for(i = 0; i < 2; i++)
    {
        if(i == 0)
            side = P_GetPtrp(li, DMU_SIDE0);
        else
            side = P_GetPtrp(li, DMU_SIDE1);

        if(!side)
            continue;

        // Versions latter than 2 store per surface texture offsets.
        if(ver >= 2)
        {
            P_SetFixedp(side, DMU_TOP_TEXTURE_OFFSET_X, SV_ReadShort() << FRACBITS);
            P_SetFixedp(side, DMU_TOP_TEXTURE_OFFSET_Y, SV_ReadShort() << FRACBITS);
            P_SetFixedp(side, DMU_MIDDLE_TEXTURE_OFFSET_X, SV_ReadShort() << FRACBITS);
            P_SetFixedp(side, DMU_MIDDLE_TEXTURE_OFFSET_Y, SV_ReadShort() << FRACBITS);
            P_SetFixedp(side, DMU_BOTTOM_TEXTURE_OFFSET_X, SV_ReadShort() << FRACBITS);
            P_SetFixedp(side, DMU_BOTTOM_TEXTURE_OFFSET_Y, SV_ReadShort() << FRACBITS);
        }
        else
        {
            fixed_t offx, offy;

            offx = SV_ReadShort() << FRACBITS;
            offy = SV_ReadShort() << FRACBITS;

            P_SetFixedp(side, DMU_TOP_TEXTURE_OFFSET_X, offx);
            P_SetFixedp(side, DMU_TOP_TEXTURE_OFFSET_Y, offy);
            P_SetFixedp(side, DMU_MIDDLE_TEXTURE_OFFSET_X, offx);
            P_SetFixedp(side, DMU_MIDDLE_TEXTURE_OFFSET_Y, offy);
            P_SetFixedp(side, DMU_BOTTOM_TEXTURE_OFFSET_X, offx);
            P_SetFixedp(side, DMU_BOTTOM_TEXTURE_OFFSET_Y, offy);
        }

        topTexID = SV_ReadShort();
        bottomTexID = SV_ReadShort();
        middleTexID = SV_ReadShort();

        if(hdr.version >= 4)
        {
            // The texture numbers are only archive numbers.
            topTexID = SV_GetArchiveTexture(topTexID);
            bottomTexID = SV_GetArchiveTexture(bottomTexID);
            middleTexID = SV_GetArchiveTexture(middleTexID);
        }

        P_SetIntp(side, DMU_TOP_TEXTURE, topTexID);
        P_SetIntp(side, DMU_BOTTOM_TEXTURE, bottomTexID);
        P_SetIntp(side, DMU_MIDDLE_TEXTURE, middleTexID);

        // Ver5 includes surface colours
        if(hdr.version >= 5)
        {
            SV_Read(rgb, 3);
            P_SetBytepv(side, DMU_TOP_COLOR, rgb);

            SV_Read(rgb, 3);
            P_SetBytepv(side, DMU_BOTTOM_COLOR, rgb);

            SV_Read(rgba, 4);
            P_SetBytepv(side, DMU_MIDDLE_COLOR, rgba);

            P_SetIntp(side, DMU_MIDDLE_BLENDMODE, SV_ReadLong());

            P_SetIntp(side, DMU_FLAGS, SV_ReadShort());
        }
    }
    // Versions > 1 might include XG data
    if(hdr.version > 1)
        if(type == lc_xg1)
            SV_ReadXGLine(li);
}

/*
 * Prepare and write the player header info.
 */
static void P_ArchivePlayerHeader(void)
{
    playerheader_t *ph = &playerHeader;
    SV_WriteByte(1); // version byte

    ph->numpowers = NUMPOWERS;
    ph->numkeys = NUMKEYS;
    ph->numfrags = MAXPLAYERS;
    ph->numweapons = NUMWEAPONS;
    ph->numammotypes = NUMAMMO;
    ph->numpsprites = NUMPSPRITES;
#if __DOOM64TC__
    ph->numartifacts = NUMARTIFACTS;
#endif

    SV_WriteLong(ph->numpowers);
    SV_WriteLong(ph->numkeys);
    SV_WriteLong(ph->numfrags);
    SV_WriteLong(ph->numweapons);
    SV_WriteLong(ph->numammotypes);
    SV_WriteLong(ph->numpsprites);
#if __DOOM64TC__
    SV_WriteLong(ph->numartifacts);
#endif
}

/*
 * Read archived playerheader info.
 */
static void P_UnArchivePlayerHeader(void)
{
    if(hdr.version >= 5)
    {
        int ver;

        ver = SV_ReadByte(); // Unused atm
        playerHeader.numpowers = SV_ReadLong();
        playerHeader.numkeys = SV_ReadLong();
        playerHeader.numfrags = SV_ReadLong();
        playerHeader.numweapons = SV_ReadLong();
        playerHeader.numammotypes = SV_ReadLong();
        playerHeader.numpsprites = SV_ReadLong();
#if __DOOM64TC__
        playerHeader.numartifacts = SV_ReadLong();
#endif
    }
    else // The old format didn't save the counts.
    {
#if __JDOOM__
        playerHeader.numpowers = 6;
        playerHeader.numkeys = 6;
        playerHeader.numfrags = 4; // why was this only 4?
        playerHeader.numweapons = 9;
        playerHeader.numammotypes = 4;
        playerHeader.numpsprites = 2;
#elif __JHERETIC__
        playerHeader.numpowers = 9;
        playerHeader.numkeys = 3;
        playerHeader.numfrags = 4; // ?
        playerHeader.numweapons = 8;
        playerHeader.numammotypes = 6;
        playerHeader.numpsprites = 2;
#endif
    }
}

static void P_ArchivePlayers(void)
{
    int     i;

    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(!players[i].plr->ingame)
            continue;
        SV_WriteLong(Net_GetPlayerID(i));
        SV_WritePlayer(i);
    }
}

static void P_UnArchivePlayers(boolean *infile, boolean *loaded)
{
    int     i;
    int     j;
    unsigned int pid;
    player_t dummy_player;
    ddplayer_t dummy_ddplayer;
    player_t *player;

    // Setup the dummy.
    dummy_player.plr = &dummy_ddplayer;

    for(i = 0; i < MAXPLAYERS; i++)
    {
        // By default a saved player translates to nothing.
        SaveToRealPlayer[i] = -1;

        if(!infile[i])
            continue;

        // The ID number will determine which player this actually is.
        pid = SV_ReadLong();
        for(player = 0, j = 0; j < MAXPLAYERS; j++)
            if((IS_NETGAME && Net_GetPlayerID(j) == pid) ||
               (!IS_NETGAME && j == 0))
            {
                // This is our guy.
                player = players + j;
                loaded[j] = true;
                // Later references to the player number 'i' must be
                // translated!
                SaveToRealPlayer[i] = j;
#ifdef _DEBUG
                Con_Printf("P_UnArchivePlayers: Saved %i is now %i.\n", i, j);
#endif
                break;
            }
        if(!player)
        {
            // We have a missing player. Use a dummy to load the data.
            player = &dummy_player;
        }
        SV_ReadPlayer(player);

        // Will be set when unarc thinker.
        player->plr->mo = NULL;
        player->attacker = NULL;
    }
}

/*
 * Only write world in the latest format
 */
static void P_ArchiveWorld(void)
{
    int     i;

    // Write the texture archives.
    SV_WriteTextureArchive();

    for(i = 0; i < numsectors; i++)
        SV_WriteSector(P_ToPtr(DMU_SECTOR, i));

    for(i = 0; i < numlines; i++)
        SV_WriteLine(P_ToPtr(DMU_LINE, i));
}

static void P_UnArchiveWorld(void)
{
    int     i;

    // Save versions >= 4 include texture archives.
    if(hdr.version >= 4)
        SV_ReadTextureArchive();

    // Load sectors.
    for(i = 0; i < numsectors; i++)
        SV_ReadSector(P_ToPtr(DMU_SECTOR, i));

    // Load lines.
    for(i = 0; i < numlines; i++)
        SV_ReadLine(P_ToPtr(DMU_LINE, i));
}

static void SV_WriteCeiling(ceiling_t* ceiling)
{
    SV_WriteByte(tc_ceiling);

    SV_WriteByte(1); // Write a version byte.

    if(ceiling->thinker.function)
        SV_WriteByte(1);
    else
        SV_WriteByte(0);

    SV_WriteByte((byte) ceiling->type);
    SV_WriteLong(P_ToIndex(ceiling->sector));

    SV_WriteShort(ceiling->bottomheight >> FRACBITS);
    SV_WriteShort(ceiling->topheight >> FRACBITS);
    SV_WriteLong(ceiling->speed);

    SV_WriteByte(ceiling->crush);

    SV_WriteLong(ceiling->direction);
    SV_WriteLong(ceiling->tag);
    SV_WriteLong(ceiling->olddirection);
}

static int SV_ReadCeiling(ceiling_t* ceiling)
{
    sector_t* sector;

    if(hdr.version >= 5)
    {   // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        // Should we set the function?
        if(SV_ReadByte())
            ceiling->thinker.function = T_MoveCeiling;

        ceiling->type = (ceiling_e) SV_ReadByte();

        sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());

        if(!sector)
            Con_Error("tc_ceiling: bad sector number\n");

        ceiling->sector = sector;

        ceiling->bottomheight = SV_ReadShort() << FRACBITS;
        ceiling->topheight = SV_ReadShort() << FRACBITS;
        ceiling->speed = SV_ReadLong();

        ceiling->crush = SV_ReadByte();

        ceiling->direction = SV_ReadLong();
        ceiling->tag = SV_ReadLong();
        ceiling->olddirection = SV_ReadLong();
    }
    else
    {
        // Its in the old pre V5 format which serialized ceiling_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, sizeof(thinker_t));

        // Start of used data members.
        SV_Read(&ceiling->type, sizeof(ceiling_e));

        // A 32bit pointer to sector, serialized.
        SV_Read(&ceiling->sector, sizeof(sector_t*));
        sector = P_ToPtr(DMU_SECTOR, (int) ceiling->sector);

        if(!sector)
            Con_Error("tc_ceiling: bad sector number\n");

        ceiling->sector = sector;

        SV_Read(&ceiling->bottomheight, sizeof(fixed_t));
        SV_Read(&ceiling->topheight, sizeof(fixed_t));
        SV_Read(&ceiling->speed, sizeof(fixed_t));
        SV_Read(&ceiling->crush, sizeof(boolean));
        SV_Read(&ceiling->direction, sizeof(int));
        SV_Read(&ceiling->tag, sizeof(int));
        SV_Read(&ceiling->olddirection, sizeof(int));

        if(junk.function)
            ceiling->thinker.function = T_MoveCeiling;
    }

    P_XSector(ceiling->sector)->specialdata = ceiling;
    return true; // Add this thinker.
}

static void SV_WriteDoor(vldoor_t* door)
{
    SV_WriteByte(tc_door);

    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteByte((byte) door->type);

    SV_WriteLong(P_ToIndex(door->sector));

    SV_WriteShort(door->topheight >> FRACBITS);
    SV_WriteLong(door->speed);

    SV_WriteLong(door->direction);
    SV_WriteLong(door->topwait);
    SV_WriteLong(door->topcountdown);
}

static int SV_ReadDoor(vldoor_t* door)
{
    sector_t* sector;

    if(hdr.version >= 5)
    {   // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        door->type = (vldoor_e) SV_ReadByte();

        sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());

        if(!sector)
            Con_Error("tc_door: bad sector number\n");

        door->sector = sector;

        door->topheight = SV_ReadShort() << FRACBITS;
        door->speed = SV_ReadLong();

        door->direction = SV_ReadLong();
        door->topwait = SV_ReadLong();
        door->topcountdown = SV_ReadLong();
    }
    else
    {
        // Its in the old pre V5 format which serialized vldoor_t
        // Padding at the start (an old thinker_t struct)
        SV_Read(junkbuffer, (size_t) 16);

        // Start of used data members.
        SV_Read(&door->type, sizeof(vldoor_e));

        // A 32bit pointer to sector, serialized.
        SV_Read(&door->sector, sizeof(sector_t*));
        sector = P_ToPtr(DMU_SECTOR, (int) door->sector);

        if(!sector)
            Con_Error("tc_door: bad sector number\n");

        door->sector = sector;

        SV_Read(&door->topheight, sizeof(fixed_t));
        SV_Read(&door->speed, sizeof(fixed_t));
        SV_Read(&door->direction, sizeof(int));
        SV_Read(&door->topwait, sizeof(int));
        SV_Read(&door->topcountdown, sizeof(int));
    }

    P_XSector(door->sector)->specialdata = door;

    door->thinker.function = T_VerticalDoor;
    return true; // Add this thinker.
}

static void SV_WriteFloor(floormove_t* floor)
{
    SV_WriteByte(tc_floor);

    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteByte((byte) floor->type);

    SV_WriteLong(P_ToIndex(floor->sector));

    SV_WriteByte((byte) floor->crush);

    SV_WriteLong(floor->direction);
    SV_WriteLong(floor->newspecial);

    SV_WriteShort(floor->texture);

    SV_WriteShort(floor->floordestheight >> FRACBITS);
    SV_WriteLong(floor->speed);
}

static int SV_ReadFloor(floormove_t* floor)
{
    sector_t* sector;

    if(hdr.version >= 5)
    {   // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        floor->type = (floor_e) SV_ReadByte();

        sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());

        if(!sector)
            Con_Error("tc_floor: bad sector number\n");

        floor->sector = sector;

        floor->crush = (boolean) SV_ReadByte();

        floor->direction = SV_ReadLong();
        floor->newspecial = SV_ReadLong();

        floor->texture = SV_ReadShort();

        floor->floordestheight = SV_ReadShort() << FRACBITS;
        floor->speed = SV_ReadLong();
    }
    else
    {
        // Its in the old pre V5 format which serialized floormove_t
        // Padding at the start (an old thinker_t struct)
        SV_Read(junkbuffer, (size_t) 16);

        // Start of used data members.
        SV_Read(&floor->type, sizeof(floor_e));

        SV_Read(&floor->crush, sizeof(boolean));

        // A 32bit pointer to sector, serialized.
        SV_Read(&floor->sector, sizeof(sector_t*));
        sector = P_ToPtr(DMU_SECTOR, (int) floor->sector);

        if(!sector)
            Con_Error("tc_floor: bad sector number\n");

        floor->sector = sector;

        SV_Read(&floor->direction, sizeof(int));
        SV_Read(&floor->newspecial, sizeof(int));
        SV_Read(&floor->texture, sizeof(short));
        SV_Read(&floor->floordestheight, sizeof(fixed_t));
        SV_Read(&floor->speed, sizeof(fixed_t));
    }

    P_XSector(floor->sector)->specialdata = floor;
    floor->thinker.function = T_MoveFloor;
    return true; // Add this thinker.
}

static void SV_WritePlat(plat_t* plat)
{
    SV_WriteByte(tc_plat);

    SV_WriteByte(1); // Write a version byte.

    if(plat->thinker.function)
        SV_WriteByte(1);
    else
        SV_WriteByte(0);

    SV_WriteByte((byte) plat->type);

    SV_WriteLong(P_ToIndex(plat->sector));

    SV_WriteLong(plat->speed);
    SV_WriteShort(plat->low >> FRACBITS);
    SV_WriteShort(plat->high >> FRACBITS);

    SV_WriteLong(plat->wait);
    SV_WriteLong(plat->count);

    SV_WriteByte((byte) plat->status);
    SV_WriteByte((byte) plat->oldstatus);
    SV_WriteByte((byte) plat->crush);

    SV_WriteLong(plat->tag);
}

static int SV_ReadPlat(plat_t* plat)
{
    sector_t* sector;

    if(hdr.version >= 5)
    {   // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        // Should we set the function?
        if(SV_ReadByte())
            plat->thinker.function = T_PlatRaise;

        plat->type = (plattype_e) SV_ReadByte();

        sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());

        if(!sector)
            Con_Error("tc_plat: bad sector number\n");

        plat->sector = sector;

        plat->speed = SV_ReadLong();
        plat->low = SV_ReadShort() << FRACBITS;
        plat->high = SV_ReadShort() << FRACBITS;

        plat->wait = SV_ReadLong();
        plat->count = SV_ReadLong();

        plat->status = (plat_e) SV_ReadByte();
        plat->oldstatus = (plat_e) SV_ReadByte();
        plat->crush = (boolean) SV_ReadByte();

        plat->tag = SV_ReadLong();
    }
    else
    {
        // Its in the old pre V5 format which serialized plat_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, sizeof(thinker_t));

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        SV_Read(&plat->sector, sizeof(sector_t*));
        sector = P_ToPtr(DMU_SECTOR, (int) plat->sector);

        if(!sector)
            Con_Error("tc_plat: bad sector number\n");

        plat->sector = sector;

        SV_Read(&plat->speed, sizeof(fixed_t));
        SV_Read(&plat->low, sizeof(fixed_t));
        SV_Read(&plat->high, sizeof(fixed_t));
        SV_Read(&plat->wait, sizeof(int));
        SV_Read(&plat->count, sizeof(int));
        SV_Read(&plat->status, sizeof(plat_e));
        SV_Read(&plat->oldstatus, sizeof(plat_e));
        SV_Read(&plat->crush, sizeof(boolean));
        SV_Read(&plat->tag, sizeof(int));
        SV_Read(&plat->type, sizeof(plattype_e));

        if(junk.function)
            plat->thinker.function = T_PlatRaise;
    }

    P_XSector(plat->sector)->specialdata = plat;
    return true; // Add this thinker.
}

static void SV_WriteFlash(lightflash_t* flash)
{
    SV_WriteByte(tc_flash);

    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(P_ToIndex(flash->sector));

    SV_WriteLong(flash->count);
    SV_WriteLong(flash->maxlight);
    SV_WriteLong(flash->minlight);
    SV_WriteLong(flash->maxtime);
    SV_WriteLong(flash->mintime);
}

static int SV_ReadFlash(lightflash_t* flash)
{
    sector_t* sector;

    if(hdr.version >= 5)
    {   // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());

        if(!sector)
            Con_Error("tc_flash: bad sector number\n");

        flash->sector = sector;

        flash->count = SV_ReadLong();
        flash->maxlight = SV_ReadLong();
        flash->minlight = SV_ReadLong();
        flash->maxtime = SV_ReadLong();
        flash->mintime = SV_ReadLong();
    }
    else
    {
        // Its in the old pre V5 format which serialized lightflash_t
        // Padding at the start (an old thinker_t struct)
        SV_Read(junkbuffer, (size_t) 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        SV_Read(&flash->sector, sizeof(sector_t*));
        sector = P_ToPtr(DMU_SECTOR, (int) flash->sector);

        if(!sector)
            Con_Error("tc_flash: bad sector number\n");

        flash->sector = sector;

        SV_Read(&flash->count, sizeof(int));
        SV_Read(&flash->maxlight, sizeof(int));
        SV_Read(&flash->minlight, sizeof(int));
        SV_Read(&flash->maxtime, sizeof(int));
        SV_Read(&flash->mintime, sizeof(int));
    }

    flash->thinker.function = T_LightFlash;
    return true; // Add this thinker.
}

static void SV_WriteStrobe(strobe_t* strobe)
{
    SV_WriteByte(tc_strobe);

    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(P_ToIndex(strobe->sector));

    SV_WriteLong(strobe->count);
    SV_WriteLong(strobe->maxlight);
    SV_WriteLong(strobe->minlight);
    SV_WriteLong(strobe->darktime);
    SV_WriteLong(strobe->brighttime);
}

static int SV_ReadStrobe(strobe_t* strobe)
{
    sector_t* sector;

    if(hdr.version >= 5)
    {   // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());

        if(!sector)
            Con_Error("tc_strobe: bad sector number\n");

        strobe->sector = sector;

        strobe->count = SV_ReadLong();
        strobe->maxlight = SV_ReadLong();
        strobe->minlight = SV_ReadLong();
        strobe->darktime = SV_ReadLong();
        strobe->brighttime = SV_ReadLong();
    }
    else
    {
        // Its in the old pre V5 format which serialized strobe_t
        // Padding at the start (an old thinker_t struct)
        SV_Read(junkbuffer, (size_t) 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        SV_Read(&strobe->sector, sizeof(sector_t*));
        sector = P_ToPtr(DMU_SECTOR, (int) strobe->sector);

        if(!sector)
            Con_Error("tc_strobe: bad sector number\n");

        strobe->sector = sector;

        SV_Read(&strobe->count, sizeof(int));
        SV_Read(&strobe->minlight, sizeof(int));
        SV_Read(&strobe->maxlight, sizeof(int));
        SV_Read(&strobe->darktime, sizeof(int));
        SV_Read(&strobe->brighttime, sizeof(int));
    }

    strobe->thinker.function = T_StrobeFlash;
    return true; // Add this thinker.
}

static void SV_WriteGlow(glow_t* glow)
{
    SV_WriteByte(tc_glow);

    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(P_ToIndex(glow->sector));

    SV_WriteLong(glow->maxlight);
    SV_WriteLong(glow->minlight);
    SV_WriteLong(glow->direction);
}

static int SV_ReadGlow(glow_t* glow)
{
    sector_t* sector;

    if(hdr.version >= 5)
    {   // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());

        if(!sector)
            Con_Error("tc_glow: bad sector number\n");

        glow->sector = sector;

        glow->maxlight = SV_ReadLong();
        glow->minlight = SV_ReadLong();
        glow->direction = SV_ReadLong();
    }
    else
    {
        // Its in the old pre V5 format which serialized strobe_t
        // Padding at the start (an old thinker_t struct)
        SV_Read(junkbuffer, (size_t) 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        SV_Read(&glow->sector, sizeof(sector_t*));
        sector = P_ToPtr(DMU_SECTOR, (int) glow->sector);

        if(!sector)
            Con_Error("tc_glow: bad sector number\n");

        glow->sector = sector;

        SV_Read(&glow->minlight, sizeof(int));
        SV_Read(&glow->maxlight, sizeof(int));
        SV_Read(&glow->direction, sizeof(int));
    }

    glow->thinker.function = T_Glow;
    return true; // Add this thinker.
}

#if __JDOOM__
static void SV_WriteFlicker(fireflicker_t* flicker)
{
    SV_WriteByte(tc_flicker);

    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(P_ToIndex(flicker->sector));

    SV_WriteLong(flicker->maxlight);
    SV_WriteLong(flicker->minlight);
}

/*
 * T_FireFlicker was added to save games in ver5,
 * therefore we don't have an old format to support
 */
static int SV_ReadFlicker(fireflicker_t* flicker)
{
    sector_t* sector;
    /*int ver =*/ SV_ReadByte(); // version byte.

    // Note: the thinker class byte has already been read.
    sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());

    if(!sector)
        Con_Error("tc_flicker: bad sector number\n");

    flicker->sector = sector;

    flicker->maxlight = SV_ReadLong();
    flicker->minlight = SV_ReadLong();

    flicker->thinker.function = T_FireFlicker;
    return true; // Add this thinker.
}
#endif

#if __DOOM64TC__
static void SV_WriteBlink(lightblink_t *blink)
{
    SV_WriteByte(tc_blink);

    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(P_ToIndex(blink->sector));

    SV_WriteLong(blink->count);
    SV_WriteLong(blink->maxlight);
    SV_WriteLong(blink->minlight);
    SV_WriteLong(blink->maxtime);
    SV_WriteLong(blink->mintime);
}

/*
 * T_LightBlink was added to save games in ver5,
 * therefore we don't have an old format to support
 */
static int SV_ReadBlink(lightblink_t *blink)
{
    sector_t* sector;
    /*int ver =*/ SV_ReadByte(); // version byte.

    // Note: the thinker class byte has already been read.
    sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());

    if(!sector)
        Con_Error("tc_lightblink: bad sector number\n");

    blink->sector = sector;

    blink->count = SV_ReadLong();
    blink->maxlight = SV_ReadLong();
    blink->minlight = SV_ReadLong();
    blink->maxtime = SV_ReadLong();
    blink->mintime = SV_ReadLong();

    blink->thinker.function = T_LightBlink;
    return true; // Add this thinker.
}
#endif

static void SV_AddThinker(thinkerclass_t tclass, thinker_t* th)
{
    P_AddThinker(&((plat_t *)th)->thinker);

    switch(tclass)
    {
    case tc_ceiling:
        P_AddActiveCeiling((ceiling_t *) th);
        break;
    case tc_plat:
        P_AddActivePlat((plat_t *) th);
        break;

    default:
        break;
    }
}

/*
 * Archives the specified thinker. Needed due to plats/ceilings in stasis.
 *
 * @param th        The thinker to be archived.
 * @param tclass    The class of the thinker to be archived.
 */
static void DoArchiveThinker(thinker_t *th, thinkerclass_t tclass)
{
    thinkerinfo_t *thInfo;

    if(!th || tclass < tc_end + 1 || tclass > NUMTHINKERCLASSES - 1)
        return;

    thInfo = &thinkerInfo[tclass];
    // Only the server saves this class of thinker?
    if((thInfo->flags & TSF_SERVERONLY) && IS_CLIENT)
        return;

    if(thInfo->Write)
        thInfo->Write(th);
}

/*
 * Archives thinkers for both client and server.
 * Clients do not save all data for all thinkers (the server will send
 * it to us anyway so saving it would just bloat client save games).
 *
 * NOTE: Some thinker classes are NEVER saved by clients.
 */
static void P_ArchiveThinkers(void)
{
    thinker_t  *th = 0;
    thinkerinfo_t *thInfo;
    boolean     found;

    // Save off the current thinkers
    for(th = thinkercap.next; th != &thinkercap && th; th = th->next)
    {
        if(th->function == NOPFUNC) // Special case for thinkers in stasis.
        {
            platlist_t *pl;
            ceilinglist_t *cl;     //jff 2/22/98 need this for ceilings too now

            // killough 2/8/98: fix plat original height bug.
            // Since acv==NULL, this could be a plat in stasis.
            // so check the active plats list, and save this
            // plat (jff: or ceiling) even if it is in stasis.
            found = false;
            for(pl = activeplats; pl && !found; pl = pl->next)
                if(pl->plat == (plat_t *) th)      // killough 2/14/98
                {
                    DoArchiveThinker(th, tc_plat);
                    found = true;
                }

            for(cl = activeceilings; cl && !found; cl = cl->next)
                if(cl->ceiling == (ceiling_t *) th)      //jff 2/22/98
                {
                    DoArchiveThinker(th, tc_ceiling);
                    found = true;
                }
        }
        else
        {
            thInfo = ThinkerInfo(th);
            if(thInfo != NULL)
                DoArchiveThinker(th, thInfo->thinkclass);
        }
    }

    // add a terminating marker
    SV_WriteByte(tc_end);
}

/*
 * Un-Archives thinkers for both client and server.
 */
static void P_UnArchiveThinkers(void)
{
    byte    tclass;
    thinker_t *th = 0;
    thinker_t *next = 0;
    thinkerinfo_t *thInfo = 0;
    boolean doSpecials = (hdr.version >= 5);
    boolean found, knownThinker;

    // remove all the current thinkers (server only)
    if(IS_SERVER)
    {
        th = thinkercap.next;
        while(th != &thinkercap)
        {
            next = th->next;

            if(th->function == P_MobjThinker)
                P_RemoveMobj((mobj_t *) th);
            else
                Z_Free(th);

            th = next;
        }

        P_InitThinkers();
    }

    // Read in saved thinkers
    for(;;)
    {
        tclass = SV_ReadByte();

        if(hdr.version < 5)
        {
            if(doSpecials) // Have we started on the specials yet?
            {
                // Versions prior to 5 used a different value to mark
                // the end of the specials data so we need to manipulate
                // the thinker class identifier value.
                if(tclass == PRE_VER5_END_SPECIALS)
                    tclass = tc_end;
                else
                    tclass += 3;
            }
            else
            {
                if(tclass == tc_end)
                {
                    // We have reached the begining of the "specials" block.
                    doSpecials = true;
                    continue;
                }
            }
        }

        if(tclass == tc_end)
            break;   // end of the list

        found = knownThinker = false;
        thInfo = thinkerInfo;
        while(thInfo->thinkclass != tc_null && !found)
        {
            if(thInfo->thinkclass == tclass)
            {
                found = true;

                // Not for us? (it shouldn't be here anyway!).
                if(!((thInfo->flags & TSF_SERVERONLY) && IS_CLIENT))
                {
                    th = Z_Malloc(thInfo->size,
                                  ((thInfo->flags & TSF_SPECIAL)? PU_LEVSPEC : PU_LEVEL),
                                  0);
                    memset(th, 0, thInfo->size);

                    knownThinker = thInfo->Read(th);
                }
            }

            thInfo++;
        }

        if(knownThinker)
            SV_AddThinker(tclass, th);

        if(!found)
            Con_Error("P_UnArchiveThinkers: Unknown tclass %i " "in savegame.",
                      tclass);
    }

    // Update references to things (server only)
    if(IS_SERVER)
    {
        mobj_t* mobj;

        for(th = thinkercap.next; th != &thinkercap; th = th->next)
        {
            if(th->function != P_MobjThinker)
                continue;
            // Update target.
            mobj = (mobj_t *) th;
            mobj->target = SV_GetArchiveThing((int) mobj->target);

#if __JDOOM__
            // Update tracer.
            mobj->tracer = SV_GetArchiveThing((int) mobj->tracer);
#endif
            // Update onmobj.
            mobj->onmobj = SV_GetArchiveThing((int) mobj->onmobj);

#if __JHERETIC__
            // Update generator.
            mobj->generator = SV_GetArchiveThing((int) mobj->generator);
#endif
        }

        // The activator mobjs must be set.
        XL_UnArchiveLines();
    }
}

#ifdef __JDOOM__
static void P_ArchiveBrain(void)
{
    int     i;

    SV_WriteByte(numbraintargets);
    SV_WriteByte(brain.targeton);
    // Write the mobj references using the mobj archive.
    for(i = 0; i < numbraintargets; i++)
        SV_WriteShort(SV_ThingArchiveNum(braintargets[i]));
}

static void P_UnArchiveBrain(void)
{
    int     i;

    if(hdr.version < 3)
        return;    // No brain data before version 3.

    numbraintargets = SV_ReadByte();
    brain.targeton = SV_ReadByte();
    for(i = 0; i < numbraintargets; i++)
        braintargets[i] = SV_GetArchiveThing(SV_ReadShort());

    if(gamemode == commercial)
        P_SpawnBrainTargets();
}
#endif

static void P_ArchiveSoundTargets(void)
{
    int         i;
    xsector_t  *xsec;

    // Write the total number
    SV_WriteLong(numSoundTargets);

    // Write the mobj references using the mobj archive.
    for(i = 0; i < numsectors; ++i)
    {
        xsec = P_XSector(P_ToPtr(DMU_SECTOR, i));

        if(xsec->soundtarget)
        {
            SV_WriteLong(i);
            SV_WriteShort(SV_ThingArchiveNum(xsec->soundtarget));
        }
    }
}

static void P_UnArchiveSoundTargets(void)
{
    int     i;
    int     secid;
    int     numsoundtargets;

    // Sound Target data was introduced in ver 5
    if(hdr.version < 5)
        return;

    // Read the number of targets
    numsoundtargets = SV_ReadLong();

    // Read in the sound targets.
    for(i = 0; i < numsoundtargets; ++i)
    {
        secid = SV_ReadLong();

        if(secid > numsectors)
            Con_Error("P_UnArchiveSoundTargets: bad sector number\n");

        P_XSector(P_ToPtr(DMU_SECTOR, secid))->soundtarget =
            SV_GetArchiveThing(SV_ReadShort());
    }
}

/*
 * Initialize the savegame directories. If the directories do not
 * exist, they are created.
 */
void SV_Init(void)
{
    if(ArgCheckWith("-savedir", 1))
    {
        strcpy(save_path, ArgNext());
        // Add a trailing backslash is necessary.
        if(save_path[strlen(save_path) - 1] != '\\')
            strcat(save_path, "\\");
    }
    else
    {
        // Use the default path.
        sprintf(save_path, "savegame\\%s\\", (char *) G_GetVariable(DD_GAME_MODE));
    }

    // Build the client save path.
    strcpy(client_save_path, save_path);
    strcat(client_save_path, "client\\");

    // Check that the save paths exist.
    M_CheckPath(save_path);
    M_CheckPath(client_save_path);
    M_TranslatePath(save_path, save_path);
    M_TranslatePath(client_save_path, client_save_path);
}

void SV_SaveGameFile(int slot, char *str)
{
    sprintf(str, "%s" SAVEGAMENAME "%i." SAVEGAMEEXTENSION, save_path, slot);
}

void SV_ClientSaveGameFile(unsigned int game_id, char *str)
{
    sprintf(str, "%s" CLIENTSAVEGAMENAME "%08X.dsg", client_save_path,
            game_id);
}

int SV_SaveGame(char *filename, char *description)
{
    int     i;

    // Open the file.
    savefile = lzOpen(filename, "wp");
    if(!savefile)
    {
        Con_Message("P_SaveGame: couldn't open \"%s\" for writing.\n",
                    filename);
        return false;           // No success.
    }

    SV_InitTextureArchives();

    // Write the header.
    hdr.magic = MY_SAVE_MAGIC;
    hdr.version = MY_SAVE_VERSION;
#ifdef __JDOOM__
    hdr.gamemode = gamemode;
#elif defined(__JHERETIC__)
    hdr.gamemode = 0;
#endif

    strncpy(hdr.description, description, SAVESTRINGSIZE);
    hdr.description[SAVESTRINGSIZE - 1] = 0;
    hdr.skill = gameskill;
#ifdef __JDOOM__
    if(fastparm)
        hdr.skill |= 0x80;      // Set high byte.
#endif
    hdr.episode = gameepisode;
    hdr.map = gamemap;
    hdr.deathmatch = deathmatch;
    hdr.nomonsters = nomonsters;
    hdr.respawn = respawnmonsters;
    hdr.leveltime = leveltime;
    hdr.gameid = SV_GameID();
    for(i = 0; i < MAXPLAYERS; i++)
        hdr.players[i] = players[i].plr->ingame;
    lzWrite(&hdr, sizeof(hdr), savefile);

    // In netgames the server tells the clients to save their games.
    NetSv_SaveGame(hdr.gameid);
    SV_InitThingArchive(false);
    P_ArchivePlayerHeader();
    P_ArchivePlayers();

    // Clear the sound target count (determined while saving sectors).
    numSoundTargets = 0;

    P_ArchiveWorld();
    P_ArchiveThinkers();

#ifdef __JDOOM__
    // Doom saves the brain data, too. (It's a part of the world.)
    P_ArchiveBrain();
#endif

    // Save the sound target data (prevents bug where monsters who have
    // been alerted go back to sleep when loading a save game).
    P_ArchiveSoundTargets();

    // To be absolutely sure...
    SV_WriteByte(CONSISTENCY);

    SV_FreeThingArchive();
    lzClose(savefile);
    return true;
}

int SV_GetSaveDescription(char *filename, char *str)
{
    savefile = lzOpen(filename, "rp");
    if(!savefile)
    {
#if __DOOM64TC__ || __WOLFTC__
        // we don't support the original game's save format (for obvious reasons).
        return false;
#else
        // It might still be a v19 savegame.
        savefile = lzOpen(filename, "r");
        if(!savefile)
            return false;       // It just doesn't exist.
        lzRead(str, SAVESTRINGSIZE, savefile);
        str[SAVESTRINGSIZE - 1] = 0;
        lzClose(savefile);
        return true;
#endif
    }
    // Read the header.
    lzRead(&hdr, sizeof(hdr), savefile);
    lzClose(savefile);
    // Check the magic.
    if(hdr.magic != MY_SAVE_MAGIC)
        // This isn't a proper savegame file.
        return false;
    strcpy(str, hdr.description);
    return true;
}

int SV_LoadGame(char *filename)
{
    int     i;
    boolean infile[MAXPLAYERS], loaded[MAXPLAYERS];
    char    buf[80];

    // Make sure an opening briefing is not shown.
    // (G_InitNew --> G_DoLoadLevel)
    brief_disabled = true;

    savefile = lzOpen(filename, "rp");
    if(!savefile)
    {
#if __DOOM64TC__ || __WOLFTC__
        // we don't support the original game's save format (for obvious reasons).
        return false;
#else
# if __JDOOM__
        // It might still be a v19 savegame.
        SV_v19_LoadGame(filename);
# elif __JHERETIC__
        SV_v13_LoadGame(filename);
# endif
#endif
        return true;
    }

    // Read the header.
    lzRead(&hdr, sizeof(hdr), savefile);
    if(hdr.magic != MY_SAVE_MAGIC)
    {
        Con_Message("SV_LoadGame: Bad magic.\n");
        return false;
    }
#ifdef __JDOOM__
    if(hdr.gamemode != gamemode && !ArgExists("-nosavecheck"))
    {
        Con_Message("SV_LoadGame: savegame not from gamemode %i.\n", gamemode);
        return false;
    }
#endif

    gameskill = hdr.skill & 0x7f;
#ifdef __JDOOM__
    fastparm = (hdr.skill & 0x80) != 0;
#endif
    gameepisode = hdr.episode;
    gamemap = hdr.map;
    deathmatch = hdr.deathmatch;
    nomonsters = hdr.nomonsters;
    respawnmonsters = hdr.respawn;
    // We don't have the right to say which players are in the game. The
    // players that already are will continue to be. If the data for a given
    // player is not in the savegame file, he will be notified. The data for
    // players who were saved but are not currently in the game will be
    // discarded.
    for(i = 0; i < MAXPLAYERS; i++)
        infile[i] = hdr.players[i];

    // Load the level.
    G_InitNew(gameskill, gameepisode, gamemap);

    // Set the time.
    leveltime = hdr.leveltime;

    // Allocate a small junk buffer.
    // (Data from old save versions is read into here)
    junkbuffer = malloc(sizeof(byte) * 64);

    // Dearchive all the data.
    memset(loaded, 0, sizeof(loaded));
    SV_InitThingArchive(true);
    P_UnArchivePlayerHeader();
    P_UnArchivePlayers(infile, loaded);
    P_UnArchiveWorld();
    P_UnArchiveThinkers();

#ifdef __JDOOM__
    // Doom saves the brain data, too. (It's a part of the world.)
    P_UnArchiveBrain();
#endif

    // Read the sound target data (prevents bug where monsters who have
    // been alerted go back to sleep when loading a save game).
    P_UnArchiveSoundTargets();

    // Check consistency.
    if(SV_ReadByte() != CONSISTENCY)
        Con_Error("SV_LoadGame: Bad savegame (consistency test failed!)\n");

    // We're done.
    SV_FreeThingArchive();
    lzClose(savefile);

    // Notify the players that weren't in the savegame.
    for(i = 0; i < MAXPLAYERS; i++)
        if(!loaded[i] && players[i].plr->ingame)
        {
            if(!i)
            {
#ifndef __JHEXEN__
#ifndef __JSTRIFE__
                P_SetMessage(players, GET_TXT(TXT_LOADMISSING), false);
#endif
#endif
            }
            else
            {
                NetSv_SendMessage(i, GET_TXT(TXT_LOADMISSING));
            }
            // Kick this player out, he doesn't belong here.
            sprintf(buf, "kick %i", i);
            DD_Execute(buf, false);
        }

    // In netgames, the server tells the clients about this.
    NetSv_LoadGame(hdr.gameid);

    // Spawn particle generators, fix HOMS etc, etc...
    R_SetupLevel("", DDSLF_AFTER_LOADING);
    return true;
}

/*
 * Saves a snapshot of the world, a still image.
 * No data of movement is included (server sends it).
 */
void SV_SaveClient(unsigned int gameid)
{
    char    name[200];
    player_t *pl = players + consoleplayer;
    mobj_t *mo = players[consoleplayer].plr->mo;

    if(!IS_CLIENT || !mo)
        return;

    SV_InitTextureArchives();

    SV_ClientSaveGameFile(gameid, name);
    // Open the file.
    savefile = lzOpen(name, "wp");
    if(!savefile)
    {
        Con_Message("SV_SaveClient: Couldn't open \"%s\" for writing.\n",
                    name);
        return;
    }
    // Prepare the header.
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = MY_CLIENT_SAVE_MAGIC;
    hdr.version = MY_SAVE_VERSION;
    hdr.skill = gameskill;
    hdr.episode = gameepisode;
    hdr.map = gamemap;
    hdr.deathmatch = deathmatch;
    hdr.nomonsters = nomonsters;
    hdr.respawn = respawnmonsters;
    hdr.leveltime = leveltime;
    hdr.gameid = gameid;
    SV_Write(&hdr, sizeof(hdr));

    // Some important information.
    // Our position and look angles.
    SV_WriteLong(mo->pos[VX]);
    SV_WriteLong(mo->pos[VY]);
    SV_WriteLong(mo->pos[VZ]);
    SV_WriteLong(mo->floorz);
    SV_WriteLong(mo->ceilingz);
    SV_WriteLong(mo->angle); /* $unifiedangles */
    SV_WriteFloat(pl->plr->lookdir); /* $unifiedangles */
    P_ArchivePlayerHeader();
    SV_WritePlayer(consoleplayer);

    P_ArchiveWorld();
    P_ArchiveThinkers();

    lzClose(savefile);
    free(junkbuffer);
}

void SV_LoadClient(unsigned int gameid)
{
    char    name[200];
    player_t *cpl = players + consoleplayer;
    mobj_t *mo = cpl->plr->mo;

    if(!IS_CLIENT || !mo)
        return;

    SV_ClientSaveGameFile(gameid, name);
    // Try to open the file.
    savefile = lzOpen(name, "rp");
    if(!savefile)
        return;

    SV_Read(&hdr, sizeof(hdr));
    if(hdr.magic != MY_CLIENT_SAVE_MAGIC)
    {
        lzClose(savefile);
        Con_Message("SV_LoadClient: Bad magic!\n");
        return;
    }

    // Allocate a small junk buffer.
    // (Data from old save versions is read into here)
    junkbuffer = malloc(sizeof(byte) * 64);

    gameskill = hdr.skill;
    deathmatch = hdr.deathmatch;
    nomonsters = hdr.nomonsters;
    respawnmonsters = hdr.respawn;
    // Do we need to change the map?
    if(gamemap != hdr.map || gameepisode != hdr.episode)
    {
        gamemap = hdr.map;
        gameepisode = hdr.episode;
        G_InitNew(gameskill, gameepisode, gamemap);
    }
    leveltime = hdr.leveltime;

    P_UnsetThingPosition(mo);
    mo->pos[VX] = SV_ReadLong();
    mo->pos[VY] = SV_ReadLong();
    mo->pos[VZ] = SV_ReadLong();
    P_SetThingPosition(mo);
    mo->floorz = SV_ReadLong();
    mo->ceilingz = SV_ReadLong();
    mo->angle = SV_ReadLong(); /* $unifiedangles */
    cpl->plr->lookdir = SV_ReadFloat(); /* $unifiedangles */
    P_UnArchivePlayerHeader();
    SV_ReadPlayer(cpl);

    P_UnArchiveWorld();
    P_UnArchiveThinkers();

    lzClose(savefile);
    free(junkbuffer);

    return;
}

#endif
