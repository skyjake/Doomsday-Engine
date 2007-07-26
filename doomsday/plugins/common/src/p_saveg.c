/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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
 * \bug Not 64bit clean: In function 'SV_ReadPlayer': cast from pointer to integer of different size
 * \bug Not 64bit clean: In function 'SV_WriteMobj': cast from pointer to integer of different size
 * \bug Not 64bit clean: In function 'RestoreMobj': cast from pointer to integer of different size
 * \bug Not 64bit clean: In function 'SV_ReadMobj': cast to pointer from integer of different size
 * \bug Not 64bit clean: In function 'P_UnArchiveThinkers': cast from pointer to integer of different size
 * \bug Not 64bit clean: In function 'P_UnArchiveBrain': cast to pointer from integer of different size, cast from pointer to integer of different size
 * \bug Not 64bit clean: In function 'P_UnArchiveSoundTargets': cast to pointer from integer of different size, cast from pointer to integer of different size
 */

/*
 * p_saveg.c: Save Game I/O
 *
 * Compiles for all supported games.
 */

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
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "p_saveg.h"
#include "f_infine.h"
#include "d_net.h"
#include "p_svtexarc.h"
#include "dmu_lib.h"
#include "p_map.h"
#include "p_mapsetup.h"
#include "p_player.h"
#include "p_inventory.h"

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
#elif __JHEXEN__
# define HXS_VERSION_TEXT      "HXS Ver " // Do not change me!
# define HXS_VERSION_TEXT_LENGTH 16

# define MY_SAVE_VERSION       4
# define SAVESTRINGSIZE        24
# define SAVEGAMENAME          "Hex"
# define CLIENTSAVEGAMENAME    "HexenCl"
# define SAVEGAMEEXTENSION     "hxs"

# define MOBJ_XX_PLAYER        -2
# define MAX_MAPS              99
# define BASE_SLOT             6
# define REBORN_SLOT           7
# define REBORN_DESCRIPTION    "TEMP GAME"
#endif

#if !__JHEXEN__
# define PRE_VER5_END_SPECIALS   7
#endif

#define FF_FULLBRIGHT       0x8000 // used to be flag in thing->frame
#define FF_FRAMEMASK        0x7fff

// TYPES -------------------------------------------------------------------

#if !__JHEXEN__
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
#endif

typedef struct playerheader_s {
    int     numpowers;
    int     numkeys;
    int     numfrags;
    int     numweapons;
    int     numammotypes;
    int     numpsprites;
#if __JHEXEN__
    int     numinvslots;
    int     numarmortypes;
#endif
#if __DOOM64TC__
    int     numartifacts;
#endif
} playerheader_t;

typedef enum gamearchivesegment_e {
    ASEG_GAME_HEADER = 101, //jhexen only
    ASEG_MAP_HEADER,        //jhexen only
    ASEG_WORLD,
    ASEG_POLYOBJS,          //jhexen only
    ASEG_MOBJS,             //jhexen < ver 4 only
    ASEG_THINKERS,
    ASEG_SCRIPTS,           //jhexen only
    ASEG_PLAYERS,
    ASEG_SOUNDS,            //jhexen only
    ASEG_MISC,              //jhexen only
    ASEG_END,
    ASEG_TEX_ARCHIVE,
    ASEG_MAP_HEADER2,       //jhexen only
    ASEG_PLAYER_HEADER
} gamearchivesegment_t;

#if __JHEXEN__
static union saveptr_u {
    byte        *b;
    short       *w;
    int         *l;
    float       *f;
} saveptr;

typedef struct targetplraddress_s {
    void     **address;
    struct targetplraddress_s *next;
} targetplraddress_t;
#endif

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

typedef enum sectorclass_e {
    sc_normal,
    sc_ploff,                   // plane offset
#if !__JHEXEN__
    sc_xg1
#endif
} sectorclass_t;

typedef enum lineclass_e {
    lc_normal,
#if !__JHEXEN__
    lc_xg1
#endif
} lineclass_t;

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

#if __JHEXEN__
static void SV_WriteLight(light_t *light);
static int  SV_ReadLight(light_t *light);
static void SV_WritePhase(phase_t *phase);
static int  SV_ReadPhase(phase_t *phase);
static void SV_WriteScript(acs_t *script);
static int  SV_ReadScript(acs_t *script);
static void SV_WriteDoorPoly(polydoor_t *polydoor);
static int  SV_ReadDoorPoly(polydoor_t *polydoor);
static void SV_WriteMovePoly(polyevent_t *movepoly);
static int  SV_ReadMovePoly(polyevent_t *movepoly);
static void SV_WriteRotatePoly(polyevent_t *rotatepoly);
static int  SV_ReadRotatePoly(polyevent_t *rotatepoly);
static void SV_WritePillar(pillar_t *pillar);
static int  SV_ReadPillar(pillar_t *pillar);
static void SV_WriteFloorWaggle(floorWaggle_t *floorwaggle);
static int  SV_ReadFloorWaggle(floorWaggle_t *floorwaggle);
#else
static void SV_WriteFlash(lightflash_t* flash);
static int  SV_ReadFlash(lightflash_t* flash);
static void SV_WriteStrobe(strobe_t* strobe);
static int  SV_ReadStrobe(strobe_t* strobe);
static void SV_WriteGlow(glow_t* glow);
static int  SV_ReadGlow(glow_t* glow);
# if __JDOOM__
static void SV_WriteFlicker(fireflicker_t* flicker);
static int  SV_ReadFlicker(fireflicker_t* flicker);
# endif

# if __DOOM64TC__
static void SV_WriteBlink(lightblink_t* flicker);
static int  SV_ReadBlink(lightblink_t* flicker);
# endif
#endif

#if __JHEXEN__
static void OpenStreamOut(char *fileName);
static void CloseStreamOut(void);

static void ClearSaveSlot(int slot);
static void CopySaveSlot(int sourceSlot, int destSlot);
static void CopyFile(char *sourceName, char *destName);
static boolean ExistingFile(char *name);

static void SV_HxLoadMap(void);
#else
static void SV_DMLoadMap(void);
#endif

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

#if __JHEXEN__
extern int ACScriptCount;
extern byte *ActionCodeBase;
extern acsinfo_t *ACSInfo;
#endif

// PUBLIC DATA DEFINITIONS -------------------------------------------------

LZFILE *savefile;
#if __JHEXEN__
char    savePath[256];        /* = "hexndata\\"; */
char    clientSavePath[256];  /* = "hexndata\\client\\"; */
#else
char    savePath[128];         /* = "savegame\\"; */
char    clientSavePath[128];  /* = "savegame\\client\\"; */
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

#if __JHEXEN__
static int saveVersion;
#else
static saveheader_t hdr;
#endif
static playerheader_t playerHeader;
static boolean playerHeaderOK = false;
static mobj_t **thing_archive = NULL;
static uint thing_archiveSize = 0;
static int saveToRealPlayerNum[MAXPLAYERS];
#if __JHEXEN__
static targetplraddress_t *targetPlayerAddrs = NULL;
static byte *saveBuffer;
static boolean savingPlayers;
#else
static int numSoundTargets = 0;
#endif

static byte *junkbuffer;        // Old save data is read into here.

static thinkerinfo_t thinkerInfo[] = {
    {
      TC_MOBJ,
      P_MobjThinker,
      TSF_SERVERONLY,
      SV_WriteMobj,
      SV_ReadMobj,
      sizeof(mobj_t)
    },
#if !__JHEXEN__
    {
      TC_XGMOVER,
      XS_PlaneMover,
      0,
      SV_WriteXGPlaneMover,
      SV_ReadXGPlaneMover,
      sizeof(xgplanemover_t)
    },
#endif
    {
      TC_CEILING,
      T_MoveCeiling,
      TSF_SPECIAL,
      SV_WriteCeiling,
      SV_ReadCeiling,
      sizeof(ceiling_t)
    },
    {
      TC_DOOR,
      T_VerticalDoor,
      TSF_SPECIAL,
      SV_WriteDoor,
      SV_ReadDoor,
      sizeof(vldoor_t)
    },
    {
      TC_FLOOR,
      T_MoveFloor,
      TSF_SPECIAL,
      SV_WriteFloor,
      SV_ReadFloor,
      sizeof(floormove_t)
    },
    {
      TC_PLAT,
      T_PlatRaise,
      TSF_SPECIAL,
      SV_WritePlat,
      SV_ReadPlat,
      sizeof(plat_t)
    },
#if __JHEXEN__
    {
     TC_INTERPRET_ACS,
     T_InterpretACS,
     0,
     SV_WriteScript,
     SV_ReadScript,
     sizeof(acs_t)
    },
    {
     TC_FLOOR_WAGGLE,
     T_FloorWaggle,
     0,
     SV_WriteFloorWaggle,
     SV_ReadFloorWaggle,
     sizeof(floorWaggle_t)
    },
    {
     TC_LIGHT,
     T_Light,
     0,
     SV_WriteLight,
     SV_ReadLight,
     sizeof(light_t)
    },
    {
     TC_PHASE,
     T_Phase,
     0,
     SV_WritePhase,
     SV_ReadPhase,
     sizeof(phase_t)
    },
    {
     TC_BUILD_PILLAR,
     T_BuildPillar,
     0,
     SV_WritePillar,
     SV_ReadPillar,
     sizeof(pillar_t)
    },
    {
     TC_ROTATE_POLY,
     T_RotatePoly,
     0,
     SV_WriteRotatePoly,
     SV_ReadRotatePoly,
     sizeof(polyevent_t)
    },
    {
     TC_MOVE_POLY,
     T_MovePoly,
     0,
     SV_WriteMovePoly,
     SV_ReadMovePoly,
     sizeof(polyevent_t)
    },
    {
     TC_POLY_DOOR,
     T_PolyDoor,
     0,
     SV_WriteDoorPoly,
     SV_ReadDoorPoly,
     sizeof(polydoor_t)
    },
#else
    {
      TC_FLASH,
      T_LightFlash,
      TSF_SPECIAL,
      SV_WriteFlash,
      SV_ReadFlash,
      sizeof(lightflash_t)
    },
    {
      TC_STROBE,
      T_StrobeFlash,
      TSF_SPECIAL,
      SV_WriteStrobe,
      SV_ReadStrobe,
      sizeof(strobe_t)
    },
    {
      TC_GLOW,
      T_Glow,
      TSF_SPECIAL,
      SV_WriteGlow,
      SV_ReadGlow,
      sizeof(glow_t)
    },
# if __JDOOM__
    {
      TC_FLICKER,
      T_FireFlicker,
      TSF_SPECIAL,
      SV_WriteFlicker,
      SV_ReadFlicker,
      sizeof(fireflicker_t)
    },
# endif
# if __DOOM64TC__
    {
      TC_BLINK,
      T_LightBlink,
      TSF_SPECIAL,
      SV_WriteBlink,
      SV_ReadBlink,
      sizeof(lightblink_t)
    },
# endif
#endif
    // Terminator
    { TC_NULL, NULL, 0, NULL, NULL, 0 }
};

// CODE --------------------------------------------------------------------

/**
 * Exit with a fatal error if the value at the current location in the save
 * file does not match that associated with the specified segment type.
 *
 * @param segType       Value by which to check for alignment.
 */
static void AssertSegment(int segType)
{
#if __JHEXEN__
    if(SV_ReadLong() != segType)
    {
        Con_Error("Corrupt save game: Segment [%d] failed alignment check",
                  segType);
    }
#endif
}

static void SV_BeginSegment(int segType)
{
#if __JHEXEN__
    SV_WriteLong(segType);
#endif
}

/**
 * @return              Ptr to the thinkerinfo for the given thinker.
 */
static thinkerinfo_t* infoForThinker(thinker_t *th)
{
    thinkerinfo_t *thInfo = thinkerInfo;

    if(!th)
        return NULL;

    while(thInfo->thinkclass != TC_NULL)
    {
        if(thInfo->function == th->function)
            return thInfo;

        thInfo++;
    }

    return NULL;
}

static thinkerinfo_t* thinkerinfo(thinkerclass_t tClass)
{
    thinkerinfo_t *thInfo = thinkerInfo;

    while(thInfo->thinkclass != TC_NULL)
    {
        if(thInfo->thinkclass == tClass)
            return thInfo;

        thInfo++;
    }

    return NULL;
}

static void removeAllThinkers(void)
{
    thinker_t *th, *next;

    th = thinkercap.next;
    while(th != &thinkercap && th)
    {
        next = th->next;

        if(th->function == P_MobjThinker)
            P_RemoveMobj((mobj_t *) th);
        else
            Z_Free(th);

        th= next;
    }
    P_InitThinkers();
}

/**
 * Must be called before saving or loading any data.
 */
static uint SV_InitThingArchive(boolean load, boolean savePlayers)
{
    uint    count = 0;

    if(load)
    {
#if !__JHEXEN__
        if(hdr.version < 5)
            count = 1024; // Limit in previous versions.
        else
#endif
            count = SV_ReadLong();
    }
    else
    {
        thinker_t *th;

        // Count the number of mobjs we'll be writing.
        th = thinkercap.next;
        while(th != &thinkercap)
        {
            if(th->function == P_MobjThinker &&
               !(((mobj_t *) th)->player && !savePlayers))
                count++;

            th = th->next;
        }
    }

    thing_archive = calloc(count, sizeof(mobj_t*));
    return thing_archiveSize = (uint) count;
}

/**
 * Used by the read code when mobjs are read.
 */
static void SV_SetArchiveThing(mobj_t *mo, int num)
{
#if __JHEXEN__
    if(saveVersion >= 4)
#endif
        num -= 1;

    if(num < 0)
        return;

    if(!thing_archive)
        Con_Error("SV_SetArchiveThing: Thing archive uninitialized.");

    thing_archive[num] = mo;
}

/**
 * Free the thing archive. Called when load is complete.
 */
static void SV_FreeThingArchive(void)
{
    if(thing_archive)
    {
        free(thing_archive);
        thing_archive = NULL;
        thing_archiveSize = 0;
    }
}

/**
 * Called by the write code to get archive numbers.
 * If the mobj is already archived, the existing number is returned.
 * Number zero is not used.
 */
#if __JHEXEN__
int SV_ThingArchiveNum(mobj_t *mo)
#else
unsigned short SV_ThingArchiveNum(mobj_t *mo)
#endif
{
    uint        i, first_empty = 0;
    boolean     found;

    // We only archive valid mobj thinkers.
    if(mo == NULL || ((thinker_t *) mo)->function != P_MobjThinker)
        return 0;

#if __JHEXEN__
    if(mo->player && !savingPlayers)
        return MOBJ_XX_PLAYER;
#endif

    if(!thing_archive)
        Con_Error("SV_ThingArchiveNum: Thing archive uninitialized.");

    found = false;
    for(i = 0; i < thing_archiveSize; ++i)
    {
        if(!thing_archive[i] && !found)
        {
            first_empty = i;
            found = true;
            continue;
        }
        if(thing_archive[i] == mo)
            return i + 1;
    }

    if(!found)
    {
        Con_Error("SV_ThingArchiveNum: Thing archive exhausted!\n");
        return 0;               // No number available!
    }

    // OK, place it in an empty pos.
    thing_archive[first_empty] = mo;
    return first_empty + 1;
}

#if __JHEXEN__
static void SV_FreeTargetPlayerList(void)
{
    targetplraddress_t *p = targetPlayerAddrs, *np;

    while(p != NULL)
    {
        np = p->next;
        free(p);
        p = np;
    }
    targetPlayerAddrs = NULL;
}
#endif

/**
 * Called by the read code to resolve mobj ptrs from archived thing ids
 * after all thinkers have been read and spawned into the map.
 */
mobj_t *SV_GetArchiveThing(int thingid, void *address)
{
#if __JHEXEN__
    if(thingid == MOBJ_XX_PLAYER)
    {
        targetplraddress_t *tpa = malloc(sizeof(targetplraddress_t));

        tpa->address = address;

        tpa->next = targetPlayerAddrs;
        targetPlayerAddrs = tpa;

        return NULL;
    }
#endif

    if(!thing_archive)
        Con_Error("SV_GetArchiveThing: Thing archive uninitialized.");

    // Check that the thing archive id is valid.
#if __JHEXEN__
    if(saveVersion < 4)
    {   // Old format is base 0.
        if(thingid == -1)
            return NULL; // A NULL reference.

        if(thingid < 0 || (uint) thingid > thing_archiveSize - 1)
            return NULL;
    }
    else
#endif
    {   // New format is base 1.
        if(thingid == 0)
            return NULL; // A NULL reference.

        if(thingid < 1 || (uint) thingid > thing_archiveSize)
        {
            Con_Message("SV_GetArchiveThing: Invalid NUM %i??\n", thingid);
            return NULL;
        }

        thingid -= 1;
    }

    return thing_archive[thingid];
}

static playerheader_t *GetPlayerHeader(void)
{
#if _DEBUG
    if(!playerHeaderOK)
        Con_Error("GetPlayerHeader: Attempted to read before init!");
#endif
    return &playerHeader;
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

#if __JHEXEN__
void SV_WriteShort(unsigned short val)
#else
void SV_WriteShort(short val)
#endif
{
    lzPutW(val, savefile);
}

#if __JHEXEN__
void SV_WriteLong(unsigned int val)
#else
void SV_WriteLong(long val)
#endif
{
    lzPutL(val, savefile);
}

void SV_WriteFloat(float val)
{
#if __JHEXEN__
    lzPutL(*(int *) &val, savefile);
#else
    long temp = 0;
    memcpy(&temp, &val, 4);
    lzPutL(temp, savefile);
#endif
}

void SV_Read(void *data, int len)
{
#if __JHEXEN__
    memcpy(data, saveptr.b, len);
    saveptr.b += len;
#else
    lzRead(data, len, savefile);
#endif
}

byte SV_ReadByte(void)
{
#if __JHEXEN__
    return (*saveptr.b++);
#else
    return lzGetC(savefile);
#endif
}

short SV_ReadShort(void)
{
#if __JHEXEN__
    return (SHORT(*saveptr.w++));
#else
    return lzGetW(savefile);
#endif
}

long SV_ReadLong(void)
{
#if __JHEXEN__
    return (LONG(*saveptr.l++));
#else
    return lzGetL(savefile);
#endif
}

float SV_ReadFloat(void)
{
#if __JHEXEN__
    return (FLOAT(*saveptr.f++));
#else
    long    val = lzGetL(savefile);
    float   returnValue = 0;

    memcpy(&returnValue, &val, 4);
    return returnValue;
#endif
}

#if __JHEXEN__
static void OpenStreamOut(char *fileName)
{
    savefile = lzOpen(fileName, "wp");
}

static void CloseStreamOut(void)
{
    if(savefile)
    {
        lzClose(savefile);
    }
}

static boolean ExistingFile(char *name)
{
    FILE       *fp;

    if((fp = fopen(name, "rb")) != NULL)
    {
        fclose(fp);
        return true;
    }
    else
    {
        return false;
    }
}

/**
 * Deletes all save game files associated with a slot number.
 */
static void ClearSaveSlot(int slot)
{
    int         i;
    char        fileName[100];

    for(i = 0; i < MAX_MAPS; ++i)
    {
        sprintf(fileName, "%shex%d%02d.hxs", savePath, slot, i);
        M_TranslatePath(fileName, fileName);
        remove(fileName);
    }
    sprintf(fileName, "%shex%d.hxs", savePath, slot);
    M_TranslatePath(fileName, fileName);
    remove(fileName);
}

static void CopyFile(char *sourceName, char *destName)
{
    size_t      length;
    byte       *buffer;
    LZFILE     *outf;

    length = M_ReadFile(sourceName, &buffer);
    outf = lzOpen(destName, "wp");
    if(outf)
    {
        lzWrite(buffer, length, outf);
        lzClose(outf);
    }
    Z_Free(buffer);
}

/**
 * Copies all the save game files from one slot to another.
 */
static void CopySaveSlot(int sourceSlot, int destSlot)
{
    int         i;
    char        sourceName[100];
    char        destName[100];

    for(i = 0; i < MAX_MAPS; ++i)
    {
        sprintf(sourceName, "%shex%d%02d.hxs", savePath, sourceSlot, i);
        M_TranslatePath(sourceName, sourceName);

        if(ExistingFile(sourceName))
        {
            sprintf(destName, "%shex%d%02d.hxs", savePath, destSlot, i);
            M_TranslatePath(destName, destName);
            CopyFile(sourceName, destName);
        }
    }

    sprintf(sourceName, "%shex%d.hxs", savePath, sourceSlot);
    M_TranslatePath(sourceName, sourceName);

    if(ExistingFile(sourceName))
    {
        sprintf(destName, "%shex%d.hxs", savePath, destSlot);
        M_TranslatePath(destName, destName);
        CopyFile(sourceName, destName);
    }
}

/**
 * Copies the base slot to the reborn slot.
 */
void SV_HxUpdateRebornSlot(void)
{
    ClearSaveSlot(REBORN_SLOT);
    CopySaveSlot(BASE_SLOT, REBORN_SLOT);
}

void SV_HxClearRebornSlot(void)
{
    ClearSaveSlot(REBORN_SLOT);
}

int SV_HxGetRebornSlot(void)
{
    return (REBORN_SLOT);
}

/**
 * @return              <code>true</code> if the reborn slot is available.
 */
boolean SV_HxRebornSlotAvailable(void)
{
    char        fileName[100];

    sprintf(fileName, "%shex%d.hxs", savePath, REBORN_SLOT);
    M_TranslatePath(fileName, fileName);
    return ExistingFile(fileName);
}

void SV_HxInitBaseSlot(void)
{
    ClearSaveSlot(BASE_SLOT);
}
#endif

/**
 * Writes the given player's data (not including the ID number).
 */
static void SV_WritePlayer(int playernum)
{
    int         i, numPSprites = GetPlayerHeader()->numpsprites;
    player_t    temp, *p = &temp;
    ddplayer_t  ddtemp, *dp = &ddtemp;

    // Make a copy of the player.
    memcpy(p, &players[playernum], sizeof(temp));
    memcpy(dp, players[playernum].plr, sizeof(ddtemp));
    temp.plr = &ddtemp;

    // Convert the psprite states.
    for(i = 0; i < numPSprites; ++i)
    {
        if(temp.psprites[i].state)
        {
            temp.psprites[i].state =
                (state_t *) (temp.psprites[i].state - states);
        }
    }

    // Version number. Increase when you make changes to the player data
    // segment format.
#if __JHEXEN__
    SV_WriteByte(2);
#else
    SV_WriteByte(3);
#endif

#if __JHEXEN__
    // Class.
    SV_WriteByte(cfg.playerClass[playernum]);
#endif

    SV_WriteLong(p->playerstate);
#if __JHEXEN__
    SV_WriteLong(p->class);    // 2nd class...?
#endif
    SV_WriteLong(FLT2FIX(dp->viewz));
    SV_WriteLong(FLT2FIX(dp->viewheight));
    SV_WriteLong(FLT2FIX(dp->deltaviewheight));
#if !__JHEXEN__
    SV_WriteFloat(dp->lookdir);
#endif
    SV_WriteLong(p->bob);
#if __JHEXEN__
    SV_WriteLong(p->flyheight);
    SV_WriteFloat(dp->lookdir);
    SV_WriteLong(p->centering);
#endif
    SV_WriteLong(p->health);

#if __JHEXEN__
    SV_Write(p->armorpoints, GetPlayerHeader()->numarmortypes * 4);
#else
    SV_WriteLong(p->armorpoints);
    SV_WriteLong(p->armortype);
#endif

#if __JHEXEN__
    for(i = 0; i < GetPlayerHeader()->numinvslots; ++i)
    {
        SV_WriteLong(p->inventory[i].type);
        SV_WriteLong(p->inventory[i].count);
    }
    SV_WriteLong(p->readyArtifact);
    SV_WriteLong(p->artifactCount);
    SV_WriteLong(p->inventorySlotNum);
# else
# if __DOOM64TC__
    SV_WriteLong(p->laserpw); // d64tc
    SV_WriteLong(p->lasericon1); // added in outcast
    SV_WriteLong(p->lasericon2); // added in outcast
    SV_WriteLong(p->lasericon3); // added in outcast
    SV_WriteLong(p->outcastcycle); // added in outcast
    SV_WriteLong(p->helltime); // added in outcast
    SV_WriteLong(p->devicetime); // added in outcast
# endif
#endif

    SV_Write(p->powers, GetPlayerHeader()->numpowers * 4);

#if __DOOM64TC__
    //SV_WriteLong(cheatenable); // added in outcast
#endif

#if __JHEXEN__
    SV_WriteLong(p->keys);
#else
    SV_Write(p->keys, GetPlayerHeader()->numkeys * 4);
#endif

#if __JHEXEN__
    SV_WriteLong(p->pieces);
#else
# if __DOOM64TC__
    SV_Write(p->artifacts, GetPlayerHeader()->numartifacts * 4);
# endif

    SV_WriteLong(p->backpack);
#endif

    SV_Write(p->frags, GetPlayerHeader()->numfrags * 4);

    SV_WriteLong(p->readyweapon);
#if !__JHEXEN__
    SV_WriteLong(p->pendingweapon);
#endif
    SV_Write(p->weaponowned, GetPlayerHeader()->numweapons * 4);
    SV_Write(p->ammo, GetPlayerHeader()->numammotypes * 4);
#if !__JHEXEN__
    SV_Write(p->maxammo, GetPlayerHeader()->numammotypes * 4);
#endif

    SV_WriteLong(p->attackdown);
    SV_WriteLong(p->usedown);

    SV_WriteLong(p->cheats);

    SV_WriteLong(p->refire);

    SV_WriteLong(p->killcount);
    SV_WriteLong(p->itemcount);
    SV_WriteLong(p->secretcount);

    SV_WriteLong(p->damagecount);
    SV_WriteLong(p->bonuscount);
#if __JHEXEN__
    SV_WriteLong(p->poisoncount);
#endif

    SV_WriteLong(dp->extralight);
    SV_WriteLong(dp->fixedcolormap);
    SV_WriteLong(p->colormap);

    SV_Write(p->psprites, numPSprites * sizeof(pspdef_t));

#if !__JHEXEN__
    SV_WriteLong(p->didsecret);

    // Added in ver 2 with __JDOOM__
    SV_WriteLong(p->flyheight);
#endif

#ifdef __JHERETIC__
    SV_Write(p->inventory, 4 * 2 * 14);
    SV_WriteLong(p->readyArtifact);
    SV_WriteLong(p->artifactCount);
    SV_WriteLong(p->inventorySlotNum);
    SV_WriteLong(p->chickenPeck);
#endif

#if __JHERETIC__ || __JHEXEN__
    SV_WriteLong(p->morphTics);
#endif

#if __JHEXEN__
    SV_WriteLong(p->jumptics);
    SV_WriteLong(p->worldTimer);
#elif __JHERETIC__
    SV_WriteLong(p->flamecount);

    // Added in ver 2
    SV_WriteByte(p->class);
#endif
}

/**
 * Reads a player's data (not including the ID number).
 */
static void SV_ReadPlayer(player_t *p)
{
    int         i, numPSprites = GetPlayerHeader()->numpsprites;
    byte        ver;
    ddplayer_t *dp = p->plr;

    ver = SV_ReadByte();

#if __JHEXEN__
    cfg.playerClass[p - players] = SV_ReadByte();

    memset(p, 0, sizeof(*p));   // Force everything NULL,
    p->plr = dp;                // but restore the ddplayer pointer.
#endif

    p->playerstate = SV_ReadLong();
#if __JHEXEN__
    p->class = SV_ReadLong();        // 2nd class...?
#endif
    dp->viewz = FIX2FLT(SV_ReadLong());
    dp->viewheight = FIX2FLT(SV_ReadLong());
    dp->deltaviewheight = FIX2FLT(SV_ReadLong());
#if !__JHEXEN__
    dp->lookdir = SV_ReadFloat();
#endif
    p->bob = SV_ReadLong();
#if __JHEXEN__
    p->flyheight = SV_ReadLong();
    dp->lookdir = SV_ReadFloat();
    p->centering = SV_ReadLong();
#endif

    p->health = SV_ReadLong();

#if __JHEXEN__
    SV_Read(p->armorpoints, GetPlayerHeader()->numarmortypes * 4);
#else
    p->armorpoints = SV_ReadLong();
    p->armortype = SV_ReadLong();
#endif

#if __JHEXEN__
    for(i = 0; i < GetPlayerHeader()->numinvslots; ++i)
    {
        p->inventory[i].type  = SV_ReadLong();
        p->inventory[i].count = SV_ReadLong();
    }
    p->readyArtifact = SV_ReadLong();
    p->artifactCount = SV_ReadLong();
    p->inventorySlotNum = SV_ReadLong();
#else
# if __DOOM64TC__
    p->laserpw = SV_ReadLong(); // d64tc
    p->lasericon1 = SV_ReadLong(); // added in outcast
    p->lasericon2 = SV_ReadLong(); // added in outcast
    p->lasericon3 = SV_ReadLong(); // added in outcast
    p->outcastcycle = SV_ReadLong(); // added in outcast
    p->helltime = SV_ReadLong(); // added in outcast
    p->devicetime = SV_ReadLong(); // added in outcast
# endif
#endif

    SV_Read(p->powers, GetPlayerHeader()->numpowers * 4);

#if __DOOM64TC__
    //cheatenable = SV_ReadLong(); // added in outcast
#endif

#if __JHEXEN__
    p->keys = SV_ReadLong();
#else
    SV_Read(p->keys, GetPlayerHeader()->numkeys * 4);
#endif

#if __JHEXEN__
    p->pieces = SV_ReadLong();
#else
# if __DOOM64TC__
    SV_Read(p->artifacts, GetPlayerHeader()->numartifacts * 4);
# endif
    p->backpack = SV_ReadLong();
#endif

    SV_Read(p->frags, GetPlayerHeader()->numfrags * 4);

#if __JHEXEN__
    p->pendingweapon = p->readyweapon = SV_ReadLong();
#else
    p->readyweapon = SV_ReadLong();
    p->pendingweapon = SV_ReadLong();
#endif

    SV_Read(p->weaponowned, GetPlayerHeader()->numweapons * 4);
    SV_Read(p->ammo, GetPlayerHeader()->numammotypes * 4);

#if !__JHEXEN__
    SV_Read(p->maxammo, GetPlayerHeader()->numammotypes * 4);
#endif

    p->attackdown = SV_ReadLong();
    p->usedown = SV_ReadLong();

    p->cheats = SV_ReadLong();

    p->refire = SV_ReadLong();

    p->killcount = SV_ReadLong();
    p->itemcount = SV_ReadLong();
    p->secretcount = SV_ReadLong();

#if __JHEXEN__
    if(ver <= 1)
    {
        /*p->messageTics =*/ SV_ReadLong();
        /*p->ultimateMessage =*/ SV_ReadLong();
        /*p->yellowMessage =*/ SV_ReadLong();
    }
#endif

    p->damagecount = SV_ReadLong();
    p->bonuscount = SV_ReadLong();
#if __JHEXEN__
    p->poisoncount = SV_ReadLong();
#endif

    dp->extralight = SV_ReadLong();
    dp->fixedcolormap = SV_ReadLong();
    p->colormap = SV_ReadLong();

    SV_Read(p->psprites, numPSprites * sizeof(pspdef_t));

#if !__JHEXEN__
    p->didsecret = SV_ReadLong();

# if __JDOOM__
    if(ver == 2) // nolonger used in >= ver 3
        /*p->messageTics =*/ SV_ReadLong();

    if(ver >= 2)
        p->flyheight = SV_ReadLong();

# elif __JHERETIC__
    if(ver < 3) // nolonger used in >= ver 3
        /*p->messageTics =*/ SV_ReadLong();

    p->flyheight = SV_ReadLong();
    SV_Read(p->inventory, 4 * 2 * 14);
    p->readyArtifact = SV_ReadLong();
    p->artifactCount = SV_ReadLong();
    p->inventorySlotNum = SV_ReadLong();
    p->chickenPeck = SV_ReadLong();
# endif
#endif

#if __JHERETIC__ || __JHEXEN__
    p->morphTics = SV_ReadLong();
#endif

#if __JHEXEN__
    p->jumptics = SV_ReadLong();
    p->worldTimer = SV_ReadLong();
#elif __JHERETIC__
    p->flamecount = SV_ReadLong();

    if(ver >= 2)
        p->class = SV_ReadByte();
#endif

#if !__JHEXEN__
    // Will be set when unarc thinker.
    p->plr->mo = NULL;
    p->attacker = NULL;
#endif

    // Demangle it.
    for(i = 0; i < numPSprites; ++i)
        if(p->psprites[i].state)
        {
            p->psprites[i].state = &states[(int) p->psprites[i].state];
        }

    // Mark the player for fixpos and fixangles.
    dp->flags |= DDPF_FIXPOS | DDPF_FIXANGLES | DDPF_FIXMOM;
    p->update |= PSF_REBORN;
}

#if __JHEXEN__
# define MOBJ_SAVEVERSION 5
#else
# define MOBJ_SAVEVERSION 7
#endif

static void SV_WriteMobj(mobj_t *original)
{
    mobj_t  temp, *mo = &temp;

    SV_WriteByte(TC_MOBJ); // Write thinker type byte.

    memcpy(mo, original, sizeof(*mo));
    // Mangle it!
    mo->state = (state_t *) (mo->state - states);
    if(mo->player)
        mo->player = (player_t *) ((mo->player - players) + 1);

    // Version.
    // JHEXEN
    // 2: Added the 'translucency' byte.
    // 3: Added byte 'vistarget'
    // 4: Added long 'tracer'
    // 4: Added long 'lastenemy'
    // 5: Added flags3
    //
    // JDOOM || JHERETIC || WOLFTC || DOOM64TC
    // 4: Added byte 'translucency'
    // 5: Added byte 'vistarget'
    // 5: Added tracer in jDoom
    // 5: Added dropoff fix in jHeretic
    // 5: Added long 'floorclip'
    // 6: Added proper respawn data
    // 6: Added flags 2 in jDoom
    // 6: Added damage
    // 7: Added generator in jHeretic
    // 7: Added flags3
    SV_WriteByte(MOBJ_SAVEVERSION);

#if !__JHEXEN__
    // A version 2 features: archive number and target.
    SV_WriteShort(SV_ThingArchiveNum(original));
    SV_WriteShort(SV_ThingArchiveNum(mo->target));

# if __JDOOM__
    // Ver 5 features: Save tracer (fixes Archvile, Revenant bug)
    SV_WriteShort(SV_ThingArchiveNum(mo->tracer));
# endif

    SV_WriteShort(SV_ThingArchiveNum(mo->onmobj));
#endif

    // Info for drawing: position.
    SV_WriteLong(mo->pos[VX]);
    SV_WriteLong(mo->pos[VY]);
    SV_WriteLong(mo->pos[VZ]);

    //More drawing info: to determine current sprite.
    SV_WriteLong(mo->angle);     // orientation
    SV_WriteLong(mo->sprite);    // used to find patch_t and flip value
    SV_WriteLong(mo->frame);

# if __JHEXEN__
    SV_WriteLong(mo->floorpic);
#else
    // The closest interval over all contacted Sectors.
    SV_WriteLong(FLT2FIX(mo->floorz));
    SV_WriteLong(FLT2FIX(mo->ceilingz));
#endif

    // For movement checking.
    SV_WriteLong(mo->radius);
    SV_WriteLong(FLT2FIX(mo->height));

    // Momentums, used to update position.
    SV_WriteLong(mo->mom[MX]);
    SV_WriteLong(mo->mom[MY]);
    SV_WriteLong(mo->mom[MZ]);

    // If == validcount, already checked.
    SV_WriteLong(mo->valid);

    SV_WriteLong(mo->type);
#if __JHEXEN__
    SV_WriteLong((int) mo->info);
#endif

    SV_WriteLong(mo->tics);      // state tic counter
    SV_WriteLong((int) mo->state);

#if __JHEXEN__
    SV_WriteLong(mo->damage);
#endif

    SV_WriteLong(mo->flags);
#if __JHEXEN__
    SV_WriteLong(mo->flags2);
    SV_WriteLong(mo->flags3);

    if(mo->type == MT_KORAX)
        SV_WriteLong(0);     // Searching index
    else
        SV_WriteLong(mo->special1);

    switch(mo->type)
    {
    case MT_LIGHTNING_FLOOR:
    case MT_LIGHTNING_ZAP:
    case MT_HOLY_TAIL:
    case MT_LIGHTNING_CEILING:
        if(mo->flags & MF_CORPSE)
            SV_WriteLong(0);
        else
            SV_WriteLong(SV_ThingArchiveNum((mobj_t *) mo->special2));
        break;

    default:
        SV_WriteLong(mo->special2);
        break;
    }
#endif
    SV_WriteLong(mo->health);

    // Movement direction, movement generation (zig-zagging).
    SV_WriteLong(mo->movedir);   // 0-7
    SV_WriteLong(mo->movecount); // when 0, select a new dir

#if __JHEXEN__
    if(mo->flags & MF_CORPSE)
        SV_WriteLong(0);
    else
        SV_WriteLong((int) SV_ThingArchiveNum(mo->target));
#endif

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    SV_WriteLong(mo->reactiontime);

#if __DOOM64TC__
    SV_WriteLong(mo->floatswitch); // added in outcast
    SV_WriteLong(mo->floattics); // added in outcast
#endif

    // If >0, the target will be chased
    // no matter what (even if shot)
    SV_WriteLong(mo->threshold);

    // Additional info record for player avatars only.
    // Only valid if type == MT_PLAYER
    SV_WriteLong((int) mo->player);

    // Player number last looked for.
    SV_WriteLong(mo->lastlook);

#if !__JHEXEN__
    // For nightmare/multiplayer respawn.
    SV_WriteLong(mo->spawninfo.pos[VX]);
    SV_WriteLong(mo->spawninfo.pos[VY]);
    SV_WriteLong(mo->spawninfo.pos[VZ]);
    SV_WriteLong(mo->spawninfo.angle);
    SV_WriteLong(mo->spawninfo.type);
    SV_WriteLong(mo->spawninfo.options);

    SV_WriteLong(mo->intflags);  // killough $dropoff_fix: internal flags
    SV_WriteLong(FLT2FIX(mo->dropoffz));  // killough $dropoff_fix
    SV_WriteLong(mo->gear);      // killough used in torque simulation

    SV_WriteLong(mo->damage);
    SV_WriteLong(mo->flags2);
    SV_WriteLong(mo->flags3);
# ifdef __JHERETIC__
    SV_WriteLong(mo->special1);
    SV_WriteLong(mo->special2);
# endif

    SV_WriteByte(mo->translucency);
    SV_WriteByte((byte)(mo->vistarget +1));
#endif

    SV_WriteLong(FLT2FIX(mo->floorclip));
#if __JHEXEN__
    SV_WriteLong(SV_ThingArchiveNum(original));
    SV_WriteLong(mo->tid);
    SV_WriteLong(mo->special);
    SV_Write(mo->args, sizeof(mo->args));
    SV_WriteByte(mo->translucency);
    SV_WriteByte((byte)(mo->vistarget +1));

    switch(mo->type)
    {
    case MT_BISH_FX:
    case MT_HOLY_FX:
    case MT_DRAGON:
    case MT_THRUSTFLOOR_UP:
    case MT_THRUSTFLOOR_DOWN:
    case MT_MINOTAUR:
    case MT_SORCFX1:
    case MT_MSTAFF_FX2:
    case MT_HOLY_TAIL:
    case MT_LIGHTNING_CEILING:
        if(mo->flags & MF_CORPSE)
            SV_WriteLong(0);
        else
            SV_WriteLong(SV_ThingArchiveNum(mo->tracer));
        break;

    default:
# if _DEBUG
if(mo->tracer != NULL)
Con_Error("SV_WriteMobj: Mobj using tracer. Possibly saved incorrectly.");
# endif
        SV_WriteLong((int) mo->tracer);
        break;
    }

    SV_WriteLong((int) mo->lastenemy);
#elif __JHERETIC__
    // Ver 7 features: generator
    SV_WriteShort(SV_ThingArchiveNum(mo->generator));
#endif
}

/**
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
#if __JHEXEN__
    // Restore DDMF flags set only in P_SpawnMobj. R_SetAllDoomsdayFlags
    // might not set these because it only iterates seclinked mobjs.
    if(mo->flags & MF_SOLID)
        mo->ddflags |= DDMF_SOLID;
    if(mo->flags2 & MF2_DONTDRAW)
        mo->ddflags |= DDMF_DONTDRAW;
#else
    if(ver < 6)
    {
        // mobj.flags
# if __JDOOM__
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
# endif
        // Remove obsoleted flags in earlier save versions.
        mo->flags &= ~MF_V6OBSOLETE;

        // mobj.flags2
# if __JDOOM__
        // jDoom only gained flags2 in ver 6 so all we can do is to
        // apply the values as set in the mobjinfo.
        // Non-persistent flags might screw things up a lot worse otherwise.
        mo->flags2 = mo->info->flags2;
# endif
    }
#endif

#if __JHEXEN__
    if(ver < 5)
#else
    if(ver < 7)
#endif
    {
        // flags3 was introduced in a latter version so all we can do is to
        // apply the values as set in the mobjinfo.
        // Non-persistent flags might screw things up a lot worse otherwise.
        mo->flags3 = mo->info->flags3;
    }
}

static boolean RestoreMobj(mobj_t *mo, int ver)
{
    mo->state = &states[(int) mo->state];
    mo->info = &mobjinfo[mo->type];
    if(mo->player)
    {
        // The player number translation table is used to find out the
        // *current* (actual) player number of the referenced player.
        int     pNum = saveToRealPlayerNum[(int) mo->player - 1];

#if __JHEXEN__
        if(pNum < 0)
        {
            // This saved player does not exist in the current game!
            // This'll make the mobj unarchiver destroy this mobj.
            Z_Free(mo);

            return false;  // Don't add this thinker.
        }
#endif

        mo->player = &players[pNum];
        mo->dplayer = mo->player->plr;
        mo->dplayer->mo = mo;
        //mo->dplayer->clAngle = mo->angle; /* $unifiedangles */
        mo->dplayer->lookdir = 0; /* $unifiedangles */
    }

    mo->visangle = mo->angle >> 16;

#if !__JHEXEN__
    if(mo->dplayer && !mo->dplayer->ingame)
    {
        if(mo->dplayer)
            mo->dplayer->mo = NULL;
        Z_Free(mo);

        return false; // Don't add this thinker.
    }
#endif

    // Do we need to update this mobj's flag values?
    if(ver < MOBJ_SAVEVERSION)
        SV_UpdateReadMobjFlags(mo, ver);

    mo->thinker.function = P_MobjThinker;

    P_SetThingPosition(mo);
    mo->floorz = P_GetFloatp(mo->subsector,
                               DMU_SECTOR_OF_SUBSECTOR | DMU_FLOOR_HEIGHT);
    mo->ceilingz = P_GetFloatp(mo->subsector,
                                 DMU_SECTOR_OF_SUBSECTOR | DMU_CEILING_HEIGHT);

    return true; // Add this thinker.
}

static int SV_ReadMobj(thinker_t *th)
{
    int         ver;
    mobj_t     *mo = (mobj_t*) th;

    ver = SV_ReadByte();

#if !__JHEXEN__
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
# if __JDOOM__
        // Tracer for enemy attacks (updated after all mobjs are loaded).
        mo->tracer = (mobj_t *) (int) SV_ReadShort();
# endif
        // mobj this one is on top of (updated after all mobjs are loaded).
        mo->onmobj = (mobj_t *) (int) SV_ReadShort();
    }
    else
    {
# if __JDOOM__
        mo->tracer = NULL;
# endif
        mo->onmobj = NULL;
    }
#endif

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

#if __JHEXEN__
    mo->floorpic = SV_ReadLong();
#else
    // The closest interval over all contacted Sectors.
    mo->floorz = FIX2FLT(SV_ReadLong());
    mo->ceilingz = FIX2FLT(SV_ReadLong());
#endif

    // For movement checking.
    mo->radius = SV_ReadLong();
    mo->height = FIX2FLT(SV_ReadLong());

    // Momentums, used to update position.
    mo->mom[MX] = SV_ReadLong();
    mo->mom[MY] = SV_ReadLong();
    mo->mom[MZ] = SV_ReadLong();

    // If == validcount, already checked.
    mo->valid = SV_ReadLong();
    mo->type = SV_ReadLong();
#if __JHEXEN__
    mo->info = (mobjinfo_t *) SV_ReadLong();
#else
    mo->info = &mobjinfo[mo->type];
#endif

    mo->tics = SV_ReadLong();   // state tic counter
    mo->state = (state_t *) SV_ReadLong();

#if __JHEXEN__
    mo->damage = SV_ReadLong();
#endif

    mo->flags = SV_ReadLong();

#if __JHEXEN__
    mo->flags2 = SV_ReadLong();
    if(ver >= 5)
        mo->flags3 = SV_ReadLong();
    mo->special1 = SV_ReadLong();
    mo->special2 = SV_ReadLong();
#endif

    mo->health = SV_ReadLong();

    // Movement direction, movement generation (zig-zagging).
    mo->movedir = SV_ReadLong();    // 0-7
    mo->movecount = SV_ReadLong();  // when 0, select a new dir

#if __JHEXEN__
    mo->target = (mobj_t *) SV_ReadLong();
#endif

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

#if __JHEXEN__
    mo->floorclip = FIX2FLT(SV_ReadLong());
    SV_SetArchiveThing(mo, SV_ReadLong());
    mo->tid = SV_ReadLong();
#else
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

# if __JDOOM__ || __WOLFTC__ || __DOOM64TC__
    if(ver >= 3)
# elif __JHERETIC__
    if(ver >= 5)
# endif
    {
        mo->intflags = SV_ReadLong();   // killough $dropoff_fix: internal flags
        mo->dropoffz = FIX2FLT(SV_ReadLong());   // killough $dropoff_fix
        mo->gear = SV_ReadLong();   // killough used in torque simulation
    }

# if __JDOOM__
    if(ver >= 6)
    {
        mo->damage = SV_ReadLong();
        mo->flags2 = SV_ReadLong();
    }// Else flags2 will be applied from the defs.
    else
        mo->damage = DDMAXINT; // Use the value set in mo->info->damage

# elif __JHERETIC__
    mo->damage = SV_ReadLong();
    mo->flags2 = SV_ReadLong();
# endif

    if(ver >= 7)
        mo->flags3 = SV_ReadLong();
    // Else flags3 will be applied from the defs.
#endif

#if __JHEXEN__
    mo->special = SV_ReadLong();
    SV_Read(mo->args, 1 * 5);
#elif __JHERETIC__
    mo->special1 = SV_ReadLong();
    mo->special2 = SV_ReadLong();
#endif

#if __JHEXEN__
    if(ver >= 2)
#else
    if(ver >= 4)
#endif
        mo->translucency = SV_ReadByte();

#if __JHEXEN__
    if(ver >= 3)
#else
    if(ver >= 5)
#endif
        mo->vistarget = (short) (SV_ReadByte()) -1;

#if __JHEXEN__
    if(ver >= 4)
        mo->tracer = (mobj_t *) SV_ReadLong();

    if(ver >= 4)
        mo->lastenemy = (mobj_t *) SV_ReadLong();
#else
    if(ver >= 5)
        mo->floorclip = FIX2FLT(SV_ReadLong());
#endif

#if __JHERETIC__
    if(ver >= 7)
        mo->generator = (mobj_t *) (int) SV_ReadShort();
    else
        mo->generator = NULL;
#endif

    // Restore! (unmangle)
    return RestoreMobj(mo, ver); // Add this thinker?
}

/**
 * Prepare and write the player header info.
 */
static void P_ArchivePlayerHeader(void)
{
    playerheader_t *ph = &playerHeader;

    SV_BeginSegment(ASEG_PLAYER_HEADER);
    SV_WriteByte(1); // version byte

    ph->numpowers = NUM_POWER_TYPES;
    ph->numkeys = NUM_KEY_TYPES;
    ph->numfrags = MAXPLAYERS;
    ph->numweapons = NUM_WEAPON_TYPES;
    ph->numammotypes = NUM_AMMO_TYPES;
    ph->numpsprites = NUMPSPRITES;
#if __JHEXEN__
    ph->numinvslots = NUMINVENTORYSLOTS;
    ph->numarmortypes = NUMARMOR;
#endif
#if __DOOM64TC__
    ph->numartifacts = NUMARTIFACTS;
#endif

    SV_WriteLong(ph->numpowers);
    SV_WriteLong(ph->numkeys);
    SV_WriteLong(ph->numfrags);
    SV_WriteLong(ph->numweapons);
    SV_WriteLong(ph->numammotypes);
    SV_WriteLong(ph->numpsprites);
#if __JHEXEN__
    SV_WriteLong(ph->numinvslots);
    SV_WriteLong(ph->numarmortypes);
#endif
#if __DOOM64TC__
    SV_WriteLong(ph->numartifacts);
#endif
    playerHeaderOK = true;
}

/**
 * Read archived player header info.
 */
static void P_UnArchivePlayerHeader(void)
{
#if __JHEXEN__
    if(saveVersion >= 4)
#else
    if(hdr.version >= 5)
#endif
    {
        int     ver;

        AssertSegment(ASEG_PLAYER_HEADER);
        ver = SV_ReadByte(); // Unused atm

        playerHeader.numpowers = SV_ReadLong();
        playerHeader.numkeys = SV_ReadLong();
        playerHeader.numfrags = SV_ReadLong();
        playerHeader.numweapons = SV_ReadLong();
        playerHeader.numammotypes = SV_ReadLong();
        playerHeader.numpsprites = SV_ReadLong();
#if __JHEXEN__
        playerHeader.numinvslots = SV_ReadLong();
        playerHeader.numarmortypes = SV_ReadLong();
#endif
#if __DOOM64TC__
        playerHeader.numartifacts = SV_ReadLong();
#endif
    }
    else // The old format didn't save the counts.
    {
#if __JHEXEN__
        playerHeader.numpowers = 9;
        playerHeader.numkeys = 11;
        playerHeader.numfrags = 8;
        playerHeader.numweapons = 4;
        playerHeader.numammotypes = 2;
        playerHeader.numpsprites = 2;
        playerHeader.numinvslots = 33;
        playerHeader.numarmortypes = 4;
#elif __JDOOM__
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
    playerHeaderOK = true;
}

static void P_ArchivePlayers(void)
{
    int         i;

    SV_BeginSegment(ASEG_PLAYERS);
#if __JHEXEN__
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        SV_WriteByte(players[i].plr->ingame);
    }
#endif

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->ingame)
            continue;
        SV_WriteLong(Net_GetPlayerID(i));
        SV_WritePlayer(i);
    }
}

static void P_UnArchivePlayers(boolean *infile, boolean *loaded)
{
    int         i, j;
    unsigned int pid;
    player_t    dummy_player;
    ddplayer_t  dummy_ddplayer;
    player_t   *player;

    // Setup the dummy.
    dummy_player.plr = &dummy_ddplayer;

    // Load the players.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        // By default a saved player translates to nothing.
        saveToRealPlayerNum[i] = -1;

        if(!infile[i])
            continue;

        // The ID number will determine which player this actually is.
        pid = SV_ReadLong();
        for(player = 0, j = 0; j < MAXPLAYERS; ++j)
            if((IS_NETGAME && Net_GetPlayerID(j) == pid) ||
               (!IS_NETGAME && j == 0))
            {
                // This is our guy.
                player = players + j;
                loaded[j] = true;
                // Later references to the player number 'i' must be
                // translated!
                saveToRealPlayerNum[i] = j;
#if _DEBUG
Con_Printf("P_UnArchivePlayers: Saved %i is now %i.\n", i, j);
#endif
                break;
            }
        if(!player)
        {
            // We have a missing player. Use a dummy to load the data.
            player = &dummy_player;
        }

        // Read the data.
        SV_ReadPlayer(player);
    }
}

static void SV_WriteSector(sector_t *sec)
{
    int         i, type;
    float       flooroffx = P_GetFloatp(sec, DMU_FLOOR_OFFSET_X);
    float       flooroffy = P_GetFloatp(sec, DMU_FLOOR_OFFSET_Y);
    float       ceiloffx = P_GetFloatp(sec, DMU_CEILING_OFFSET_X);
    float       ceiloffy = P_GetFloatp(sec, DMU_CEILING_OFFSET_Y);
    byte        lightlevel = (byte) (P_GetFloatp(sec, DMU_LIGHT_LEVEL) / 255.0f);
    short       floorheight = (short) P_GetIntp(sec, DMU_FLOOR_HEIGHT);
    short       ceilingheight = (short) P_GetIntp(sec, DMU_CEILING_HEIGHT);
    int         floorpic = P_GetIntp(sec, DMU_FLOOR_TEXTURE);
    int         ceilingpic = P_GetIntp(sec, DMU_CEILING_TEXTURE);
    xsector_t  *xsec = P_XSector(sec);
    float       rgb[3];

#if !__JHEXEN__
    // Determine type.
    if(xsec->xg)
        type = sc_xg1;
    else
#endif
        if(flooroffx || flooroffy || ceiloffx || ceiloffy)
        type = sc_ploff;
    else
        type = sc_normal;

    // Type byte.
    SV_WriteByte(type);

    // Version.
    // 2: Surface colors.
    SV_WriteByte(2); // write a version byte.

    SV_WriteShort(floorheight);
    SV_WriteShort(ceilingheight);
    SV_WriteShort(SV_FlatArchiveNum(floorpic));
    SV_WriteShort(SV_FlatArchiveNum(ceilingpic));
#if __JHEXEN__
    SV_WriteShort((short) lightlevel);
#else
    SV_WriteByte(lightlevel);
#endif

    P_GetFloatpv(sec, DMU_COLOR, rgb);
    for(i = 0; i < 3; ++i)
        SV_WriteByte((byte)(255.f * rgb[i]));

    P_GetFloatpv(sec, DMU_FLOOR_COLOR, rgb);
    for(i = 0; i < 3; ++i)
        SV_WriteByte((byte)(255.f * rgb[i]));

    P_GetFloatpv(sec, DMU_CEILING_COLOR, rgb);
    for(i = 0; i < 3; ++i)
        SV_WriteByte((byte)(255.f * rgb[i]));

    SV_WriteShort(xsec->special);
    SV_WriteShort(xsec->tag);

#if __JHEXEN__
    SV_WriteShort(xsec->seqType);
#endif

    if(type == sc_ploff
#if !__JHEXEN__
       || type == sc_xg1
#endif
       )
    {
        SV_WriteFloat(flooroffx);
        SV_WriteFloat(flooroffy);
        SV_WriteFloat(ceiloffx);
        SV_WriteFloat(ceiloffy);
    }

#if !__JHEXEN__
    if(xsec->xg)                 // Extended General?
    {
        SV_WriteXGSector(sec);
    }

    // Count the number of sound targets
    if(xsec->soundtarget)
        numSoundTargets++;
#endif
}

/**
 * Reads all versions of archived sectors.
 * Including the old Ver1.
 */
static void SV_ReadSector(sector_t *sec)
{
    int         i, ver = 1;
    int         type = 0;
    int         floorTexID;
    int         ceilingTexID;
    byte        rgb[3], lightlevel;
    xsector_t  *xsec = P_XSector(sec);
    int         fh, ch;

    // A type byte?
#if __JHEXEN__
    if(saveVersion < 4)
        type = sc_ploff;
    else
#else
    if(hdr.version <= 1)
        type = sc_normal;
    else
#endif
        type = SV_ReadByte();

    // A version byte?
#if __JHEXEN__
    if(saveVersion > 2)
#else
    if(hdr.version > 4)
#endif
        ver = SV_ReadByte();

    fh = SV_ReadShort();
    ch = SV_ReadShort();

    P_SetIntp(sec, DMU_FLOOR_HEIGHT, fh);
    P_SetIntp(sec, DMU_CEILING_HEIGHT, ch);

#if __JHEXEN__
    // Update the "target heights" of the planes.
    P_SetIntp(sec, DMU_FLOOR_TARGET, fh);
    P_SetIntp(sec, DMU_CEILING_TARGET, ch);
    // The move speed is not saved; can cause minor problems.
    P_SetIntp(sec, DMU_FLOOR_SPEED, 0);
    P_SetIntp(sec, DMU_CEILING_SPEED, 0);
#endif

    floorTexID = SV_ReadShort();
    ceilingTexID = SV_ReadShort();

#if !__JHEXEN__
    if(hdr.version == 1)
    {
        // The flat numbers are the actual lump numbers.
        int     firstflat = W_CheckNumForName("F_START") + 1;

        floorTexID += firstflat;
        ceilingTexID += firstflat;
    }
    else if(hdr.version >= 4)
#endif
    {
        // The flat numbers are actually archive numbers.
        floorTexID = SV_GetArchiveFlat(floorTexID);
        ceilingTexID = SV_GetArchiveFlat(ceilingTexID);
    }

    P_SetIntp(sec, DMU_FLOOR_TEXTURE, floorTexID);
    P_SetIntp(sec, DMU_CEILING_TEXTURE, ceilingTexID);

#if __JHEXEN__
    lightlevel = (byte) SV_ReadShort();
#else
    // In Ver1 the light level is a short
    if(hdr.version == 1)
        lightlevel = (byte) SV_ReadShort();
    else
        lightlevel = SV_ReadByte();
#endif
    P_SetFloatp(sec, DMU_LIGHT_LEVEL, (float) lightlevel / 255.f);

#if !__JHEXEN__
    if(hdr.version > 1)
#endif
    {
        SV_Read(rgb, 3);
        for(i = 0; i < 3; ++i)
            P_SetFloatp(sec, DMU_COLOR_RED + i, rgb[i] / 255.f);
    }

    // Ver 2 includes surface colours
    if(ver >= 2)
    {
        SV_Read(rgb, 3);
        for(i = 0; i < 3; ++i)
            P_SetFloatp(sec, DMU_FLOOR_COLOR_RED + i, rgb[i] / 255.f);

        SV_Read(rgb, 3);
        for(i = 0; i < 3; ++i)
            P_SetFloatp(sec, DMU_CEILING_COLOR_RED + i, rgb[i] / 255.f);
    }

    xsec->special = SV_ReadShort();
    /*xsec->tag =*/ SV_ReadShort();

#if __JHEXEN__
    xsec->seqType = SV_ReadShort();
#endif

    if(type == sc_ploff
#if !__JHEXEN__
       || type == sc_xg1
#endif
       )
    {
        P_SetFloatp(sec, DMU_FLOOR_OFFSET_X, SV_ReadFloat());
        P_SetFloatp(sec, DMU_FLOOR_OFFSET_Y, SV_ReadFloat());
        P_SetFloatp(sec, DMU_CEILING_OFFSET_X, SV_ReadFloat());
        P_SetFloatp(sec, DMU_CEILING_OFFSET_Y, SV_ReadFloat());
    }

#if !__JHEXEN__
    if(type == sc_xg1)
        SV_ReadXGSector(sec);
#endif

#if !__JHEXEN__
    if(hdr.version <= 1)
#endif
    {
        xsec->specialdata = 0;
    }

    // We'll restore the sound targets latter on
    xsec->soundtarget = 0;
}

static void SV_WriteLine(line_t *li)
{
    uint        i, j;
    int         texid;
    float       rgba[4];
    lineclass_t type;
    xline_t    *xli = P_XLine(li);

#if !__JHEXEN__
    if(xli->xg)
        type =  lc_xg1;
    else
#endif
        type = lc_normal;
    SV_WriteByte(type);

    // Version.
    // 2: Per surface texture offsets.
    // 2: Surface colors.
    // 3: "Mapped by player" values.
    SV_WriteByte(3); // Write a version byte

    SV_WriteShort(P_GetIntp(li, DMU_FLAGS));

    for(i = 0; i < MAXPLAYERS; ++i)
        SV_WriteByte(xli->mapped[i]);

#if __JHEXEN__
    SV_WriteByte(xli->special);
    SV_WriteByte(xli->arg1);
    SV_WriteByte(xli->arg2);
    SV_WriteByte(xli->arg3);
    SV_WriteByte(xli->arg4);
    SV_WriteByte(xli->arg5);
#else
    SV_WriteShort(xli->special);
    SV_WriteShort(xli->tag);
#endif

    // For each side
    for(i = 0; i < 2; ++i)
    {
        side_t *si = P_GetPtrp(li, (i? DMU_SIDE1:DMU_SIDE0));
        if(!si)
            continue;

        SV_WriteShort(P_GetIntp(si, DMU_TOP_TEXTURE_OFFSET_X));
        SV_WriteShort(P_GetIntp(si, DMU_TOP_TEXTURE_OFFSET_Y));
        SV_WriteShort(P_GetIntp(si, DMU_MIDDLE_TEXTURE_OFFSET_X));
        SV_WriteShort(P_GetIntp(si, DMU_MIDDLE_TEXTURE_OFFSET_Y));
        SV_WriteShort(P_GetIntp(si, DMU_BOTTOM_TEXTURE_OFFSET_X));
        SV_WriteShort(P_GetIntp(si, DMU_BOTTOM_TEXTURE_OFFSET_Y));

        texid = P_GetIntp(si, DMU_TOP_TEXTURE);
        SV_WriteShort(SV_TextureArchiveNum(texid));

        texid = P_GetIntp(si, DMU_BOTTOM_TEXTURE);
        SV_WriteShort(SV_TextureArchiveNum(texid));

        texid = P_GetIntp(si, DMU_MIDDLE_TEXTURE);
        SV_WriteShort(SV_TextureArchiveNum(texid));

        P_GetFloatpv(si, DMU_TOP_COLOR, rgba);
        for(j = 0; j < 3; ++j)
            SV_WriteByte((byte)(255 * rgba[j]));

        P_GetFloatpv(si, DMU_BOTTOM_COLOR, rgba);
        for(j = 0; j < 3; ++j)
            SV_WriteByte((byte)(255 * rgba[j]));

        P_GetFloatpv(si, DMU_MIDDLE_COLOR, rgba);
        for(j = 0; j < 3; ++j)
            SV_WriteByte((byte)(255 * rgba[j]));

        SV_WriteLong(P_GetIntp(si, DMU_MIDDLE_BLENDMODE));
        SV_WriteShort(P_GetIntp(si, DMU_FLAGS));
    }

#if !__JHEXEN__
    // Extended General?
    if(xli->xg)
    {
        SV_WriteXGLine(li);
    }
#endif
}

/**
 * Reads all versions of archived lines.
 * Including the old Ver1.
 */
static void SV_ReadLine(line_t *li)
{
    int         i, j;
    lineclass_t type;
    int         ver;
    int         topTexID;
    int         bottomTexID;
    int         middleTexID;
    byte        rgba[4];
    short       flags;
    xline_t    *xli = P_XLine(li);

    // A type byte?
#if __JHEXEN__
    if(saveVersion < 4)
#else
    if(hdr.version < 2)
#endif
        type = lc_normal;
    else
        type = (int) SV_ReadByte();

    // A version byte?
#if __JHEXEN__
    if(saveVersion < 3)
#else
    if(hdr.version < 5)
#endif
        ver = 1;
    else
        ver = (int) SV_ReadByte();

    flags = SV_ReadShort();
    if(ver < 3 && (flags & 0x0100)) // the old ML_MAPPED flag
    {
        // Set line as having been seen by all players..
        memset(&xli->mapped, 1, sizeof(&xli->mapped));
        flags &= ~0x0100; // remove the old flag.
    }
    P_SetIntp(li, DMU_FLAGS, flags);

    if(ver >= 3)
    {
        for(i = 0; i < MAXPLAYERS; ++i)
            xli->mapped[i] = SV_ReadByte();
    }

#if __JHEXEN__
    xli->special = SV_ReadByte();
    xli->arg1 = SV_ReadByte();
    xli->arg2 = SV_ReadByte();
    xli->arg3 = SV_ReadByte();
    xli->arg4 = SV_ReadByte();
    xli->arg5 = SV_ReadByte();
#else
    xli->special = SV_ReadShort();
    /*xli->tag =*/ SV_ReadShort();
#endif

    // For each side
    for(i = 0; i < 2; ++i)
    {
        side_t *si = P_GetPtrp(li, (i? DMU_SIDE1:DMU_SIDE0));
        if(!si)
            continue;

        // Versions latter than 2 store per surface texture offsets.
        if(ver >= 2)
        {
            P_SetFloatp(si, DMU_TOP_TEXTURE_OFFSET_X, (float) SV_ReadShort());
            P_SetFloatp(si, DMU_TOP_TEXTURE_OFFSET_Y, (float) SV_ReadShort());
            P_SetFloatp(si, DMU_MIDDLE_TEXTURE_OFFSET_X, (float) SV_ReadShort());
            P_SetFloatp(si, DMU_MIDDLE_TEXTURE_OFFSET_Y, (float) SV_ReadShort());
            P_SetFloatp(si, DMU_BOTTOM_TEXTURE_OFFSET_X, (float) SV_ReadShort());
            P_SetFloatp(si, DMU_BOTTOM_TEXTURE_OFFSET_Y, (float) SV_ReadShort());
        }
        else
        {
            float offx, offy;

            offx = (float) SV_ReadShort();
            offy = (float) SV_ReadShort();

            P_SetFloatp(si, DMU_TOP_TEXTURE_OFFSET_X, offx);
            P_SetFloatp(si, DMU_TOP_TEXTURE_OFFSET_Y, offy);
            P_SetFloatp(si, DMU_MIDDLE_TEXTURE_OFFSET_X, offx);
            P_SetFloatp(si, DMU_MIDDLE_TEXTURE_OFFSET_Y, offy);
            P_SetFloatp(si, DMU_BOTTOM_TEXTURE_OFFSET_X, offx);
            P_SetFloatp(si, DMU_BOTTOM_TEXTURE_OFFSET_Y, offy);
        }

        topTexID = SV_ReadShort();
        bottomTexID = SV_ReadShort();
        middleTexID = SV_ReadShort();

#if !__JHEXEN__
        if(hdr.version >= 4)
#endif
        {
            // The texture numbers are archive numbers.
            topTexID = SV_GetArchiveTexture(topTexID);
            bottomTexID = SV_GetArchiveTexture(bottomTexID);
            middleTexID = SV_GetArchiveTexture(middleTexID);
        }

        P_SetIntp(si, DMU_TOP_TEXTURE, topTexID);
        P_SetIntp(si, DMU_BOTTOM_TEXTURE, bottomTexID);
        P_SetIntp(si, DMU_MIDDLE_TEXTURE, middleTexID);

        // Ver2 includes surface colours
        if(ver >= 2)
        {
            SV_Read(rgba, 3);
            for(j = 0; j < 3; ++j)
                P_SetFloatp(si, DMU_TOP_COLOR_RED + j, rgba[j] / 255.f);

            SV_Read(rgba, 3);
            for(j = 0; j < 3; ++j)
                P_SetFloatp(si, DMU_BOTTOM_COLOR + j, rgba[j] / 255.f);

            SV_Read(rgba, 4);
            for(j = 0; j < 3; ++j)
                P_SetFloatp(si, DMU_MIDDLE_COLOR + j, rgba[j] / 255.f);

            P_SetIntp(si, DMU_MIDDLE_BLENDMODE, SV_ReadLong());
            P_SetIntp(si, DMU_FLAGS, SV_ReadShort());
        }
    }

#if !__JHEXEN__
    if(type == lc_xg1)
        SV_ReadXGLine(li);
#endif
}

#if __JHEXEN__
static void SV_WritePolyObj(polyobj_t *po)
{
    SV_WriteByte(1); // write a version byte.

    SV_WriteLong(P_GetIntp(po, DMU_TAG));
    SV_WriteLong(P_GetAnglep(po, DMU_ANGLE));
    SV_WriteLong(P_GetFixedp(po, DMU_START_SPOT_X));
    SV_WriteLong(P_GetFixedp(po, DMU_START_SPOT_Y));
}

static int SV_ReadPolyObj(polyobj_t *po)
{
    int         ver;
    fixed_t     deltaX;
    fixed_t     deltaY;
    angle_t     angle;

    if(saveVersion >= 3)
        ver = SV_ReadByte();

    if(SV_ReadLong() != P_GetIntp(po, DMU_TAG))
        Con_Error("UnarchivePolyobjs: Invalid polyobj tag");

    angle = (angle_t) SV_ReadLong();
    PO_RotatePolyobj(P_GetIntp(po, DMU_TAG), angle);
    P_SetAnglep(po, DMU_DESTINATION_ANGLE, angle);
    deltaX = SV_ReadLong() - P_GetFixedp(po, DMU_START_SPOT_X);
    deltaY = SV_ReadLong() - P_GetFixedp(po, DMU_START_SPOT_Y);
    PO_MovePolyobj(P_GetIntp(po, DMU_TAG), deltaX, deltaY);
    //// \fixme What about speed? It isn't saved at all?
    return true;
}
#endif

/**
 * Only write world in the latest format.
 */
static void P_ArchiveWorld(void)
{
    uint        i;

    SV_BeginSegment(ASEG_TEX_ARCHIVE);
    SV_WriteTextureArchive();

    SV_BeginSegment(ASEG_WORLD);
    for(i = 0; i < numsectors; ++i)
        SV_WriteSector(P_ToPtr(DMU_SECTOR, i));

    for(i = 0; i < numlines; ++i)
        SV_WriteLine(P_ToPtr(DMU_LINE, i));

#if __JHEXEN__
    SV_BeginSegment(ASEG_POLYOBJS);
    SV_WriteLong(numpolyobjs);
    for(i = 0; i < numpolyobjs; ++i)
        SV_WritePolyObj(P_ToPtr(DMU_POLYOBJ, i));
#endif
}

static void P_UnArchiveWorld(void)
{
    uint        i;

    AssertSegment(ASEG_TEX_ARCHIVE);

    // Load texturearchive?
#if !__JHEXEN__
    if(hdr.version >= 4)
#endif
        SV_ReadTextureArchive();

    AssertSegment(ASEG_WORLD);
    // Load sectors.
    for(i = 0; i < numsectors; ++i)
        SV_ReadSector(P_ToPtr(DMU_SECTOR, i));
    // Load lines.
    for(i = 0; i < numlines; ++i)
        SV_ReadLine(P_ToPtr(DMU_LINE, i));

#if __JHEXEN__
    // Load polyobjects.
    AssertSegment(ASEG_POLYOBJS);
    if(SV_ReadLong() != numpolyobjs)
        Con_Error("UnarchivePolyobjs: Bad polyobj count");

    for(i = 0; i < numpolyobjs; ++i)
        SV_ReadPolyObj(P_ToPtr(DMU_POLYOBJ, i));
#endif
}

static void SV_WriteCeiling(ceiling_t *ceiling)
{
    SV_WriteByte(TC_CEILING);

    SV_WriteByte(1); // Write a version byte.

    if(ceiling->thinker.function)
        SV_WriteByte(1);
    else
        SV_WriteByte(0);

    SV_WriteByte((byte) ceiling->type);
    SV_WriteLong(P_ToIndex(ceiling->sector));

    SV_WriteShort((int)ceiling->bottomheight);
    SV_WriteShort((int)ceiling->topheight);
    SV_WriteLong(FLT2FIX(ceiling->speed));

    SV_WriteByte(ceiling->crush);

    SV_WriteLong(ceiling->direction);
    SV_WriteLong(ceiling->tag);
    SV_WriteLong(ceiling->olddirection);
}

static int SV_ReadCeiling(ceiling_t *ceiling)
{
    sector_t *sector;

#if __JHEXEN__
    if(saveVersion >= 4)
#else
    if(hdr.version >= 5)
#endif
    {   // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        // Should we set the function?
        if(SV_ReadByte())
            ceiling->thinker.function = T_MoveCeiling;

        ceiling->type = (ceiling_e) SV_ReadByte();

        sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());

        if(!sector)
            Con_Error("TC_CEILING: bad sector number\n");

        ceiling->sector = sector;

        ceiling->bottomheight = (float) SV_ReadShort();
        ceiling->topheight = (float) SV_ReadShort();
        ceiling->speed = FIX2FLT((fixed_t) SV_ReadLong());

        ceiling->crush = SV_ReadByte();

        ceiling->direction = SV_ReadLong();
        ceiling->tag = SV_ReadLong();
        ceiling->olddirection = SV_ReadLong();
    }
    else
    {
        // Its in the old format which serialized ceiling_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
#if __JHEXEN__
        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());
        if(!sector)
            Con_Error("TC_CEILING: bad sector number\n");
        ceiling->sector = sector;

        ceiling->type = SV_ReadLong();
#else
        ceiling->type = SV_ReadLong();

        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());
        if(!sector)
            Con_Error("TC_CEILING: bad sector number\n");
        ceiling->sector = sector;
#endif

        ceiling->bottomheight = FIX2FLT((fixed_t) SV_ReadLong());
        ceiling->topheight = FIX2FLT((fixed_t) SV_ReadLong());
        ceiling->speed = FIX2FLT((fixed_t) SV_ReadLong());

        ceiling->crush = SV_ReadLong();
        ceiling->direction = SV_ReadLong();
        ceiling->tag = SV_ReadLong();
        ceiling->olddirection = SV_ReadLong();

#if !__JHEXEN__
        if(junk.function)
#endif
            ceiling->thinker.function = T_MoveCeiling;
    }

    P_XSector(ceiling->sector)->specialdata = ceiling;
    return true; // Add this thinker.
}

static void SV_WriteDoor(vldoor_t *door)
{
    SV_WriteByte(TC_DOOR);

    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteByte((byte) door->type);

    SV_WriteLong(P_ToIndex(door->sector));

    SV_WriteShort((int)door->topheight);
    SV_WriteLong(FLT2FIX(door->speed));

    SV_WriteLong(door->direction);
    SV_WriteLong(door->topwait);
    SV_WriteLong(door->topcountdown);
}

static int SV_ReadDoor(vldoor_t *door)
{
    sector_t *sector;

#if __JHEXEN__
    if(saveVersion >= 4)
#else
    if(hdr.version >= 5)
#endif
    {   // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        door->type = (vldoor_e) SV_ReadByte();

        sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());

        if(!sector)
            Con_Error("TC_DOOR: bad sector number\n");

        door->sector = sector;

        door->topheight = (float) SV_ReadShort();
        door->speed = FIX2FLT((fixed_t) SV_ReadLong());

        door->direction = SV_ReadLong();
        door->topwait = SV_ReadLong();
        door->topcountdown = SV_ReadLong();
    }
    else
    {
        // Its in the old format which serialized vldoor_t
        // Padding at the start (an old thinker_t struct)
        SV_Read(junkbuffer, (size_t) 16);

        // Start of used data members.
#if __JHEXEN__
        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_DOOR: bad sector number\n");
        door->sector = sector;

        door->type = SV_ReadLong();
#else
        door->type = SV_ReadLong();

        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_DOOR: bad sector number\n");
        door->sector = sector;
#endif
        door->topheight = FIX2FLT((fixed_t) SV_ReadLong());
        door->speed = FIX2FLT((fixed_t) SV_ReadLong());

        door->direction = SV_ReadLong();
        door->topwait = SV_ReadLong();
        door->topcountdown = SV_ReadLong();
    }

    P_XSector(door->sector)->specialdata = door;
    door->thinker.function = T_VerticalDoor;

    return true; // Add this thinker.
}

static void SV_WriteFloor(floormove_t *floor)
{
    SV_WriteByte(TC_FLOOR);

    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteByte((byte) floor->type);

    SV_WriteLong(P_ToIndex(floor->sector));

    SV_WriteByte((byte) floor->crush);

    SV_WriteLong(floor->direction);
    SV_WriteLong(floor->newspecial);

    SV_WriteShort(floor->texture);

    SV_WriteShort((int)floor->floordestheight);
    SV_WriteLong(FLT2FIX(floor->speed));

#if __JHEXEN__
    SV_WriteLong(floor->delayCount);
    SV_WriteLong(floor->delayTotal);
    SV_WriteLong(FLT2FIX(floor->stairsDelayHeight));
    SV_WriteLong(FLT2FIX(floor->stairsDelayHeightDelta));
    SV_WriteLong(FLT2FIX(floor->resetHeight));
    SV_WriteShort(floor->resetDelay);
    SV_WriteShort(floor->resetDelayCount);
    SV_WriteByte(floor->textureChange);
#endif
}

static int SV_ReadFloor(floormove_t *floor)
{
    sector_t *sector;

#if __JHEXEN__
    if(saveVersion >= 4)
#else
    if(hdr.version >= 5)
#endif
    {   // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        floor->type = (floor_e) SV_ReadByte();

        sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());

        if(!sector)
            Con_Error("TC_FLOOR: bad sector number\n");

        floor->sector = sector;

        floor->crush = (boolean) SV_ReadByte();

        floor->direction = SV_ReadLong();
        floor->newspecial = SV_ReadLong();

        floor->texture = SV_ReadShort();

        floor->floordestheight = (float) SV_ReadShort();
        floor->speed = FIX2FLT(SV_ReadLong());

#if __JHEXEN__
        floor->delayCount = SV_ReadLong();
        floor->delayTotal = SV_ReadLong();
        floor->stairsDelayHeight = FIX2FLT(SV_ReadLong());
        floor->stairsDelayHeightDelta = FIX2FLT(SV_ReadLong());
        floor->resetHeight = FIX2FLT(SV_ReadLong());
        floor->resetDelay = SV_ReadShort();
        floor->resetDelayCount = SV_ReadShort();
        floor->textureChange = SV_ReadByte();
#endif
    }
    else
    {
        // Its in the old format which serialized floormove_t
        // Padding at the start (an old thinker_t struct)
        SV_Read(junkbuffer, (size_t) 16);

        // Start of used data members.
#if __JHEXEN__
        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_FLOOR: bad sector number\n");
        floor->sector = sector;

        floor->type = SV_ReadLong();
        floor->crush = SV_ReadLong();
#else
        floor->type = SV_ReadLong();

        floor->crush = SV_ReadLong();

        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_FLOOR: bad sector number\n");
        floor->sector = sector;
#endif
        floor->direction = SV_ReadLong();
        floor->newspecial = SV_ReadLong();
        floor->texture = SV_ReadShort();

        floor->floordestheight = FIX2FLT((fixed_t) SV_ReadLong());
        floor->speed = FIX2FLT((fixed_t) SV_ReadLong());

#if __JHEXEN__
        floor->delayCount = SV_ReadLong();
        floor->delayTotal = SV_ReadLong();
        floor->stairsDelayHeight = FIX2FLT((fixed_t) SV_ReadLong());
        floor->stairsDelayHeightDelta = FIX2FLT((fixed_t) SV_ReadLong());
        floor->resetHeight = FIX2FLT((fixed_t) SV_ReadLong());
        floor->resetDelay = SV_ReadShort();
        floor->resetDelayCount = SV_ReadShort();
        floor->textureChange = SV_ReadByte();
#endif
    }

    P_XSector(floor->sector)->specialdata = floor;
    floor->thinker.function = T_MoveFloor;

    return true; // Add this thinker.
}

static void SV_WritePlat(plat_t *plat)
{
    SV_WriteByte(TC_PLAT);

    SV_WriteByte(1); // Write a version byte.

    if(plat->thinker.function)
        SV_WriteByte(1);
    else
        SV_WriteByte(0);

    SV_WriteByte((byte) plat->type);

    SV_WriteLong(P_ToIndex(plat->sector));

    SV_WriteLong(FLT2FIX(plat->speed));
    SV_WriteShort((int)plat->low);
    SV_WriteShort((int)plat->high);

    SV_WriteLong(plat->wait);
    SV_WriteLong(plat->count);

    SV_WriteByte((byte) plat->status);
    SV_WriteByte((byte) plat->oldstatus);
    SV_WriteByte((byte) plat->crush);

    SV_WriteLong(plat->tag);
}

static int SV_ReadPlat(plat_t *plat)
{
    sector_t *sector;

#if __JHEXEN__
    if(saveVersion >= 4)
#else
    if(hdr.version >= 5)
#endif
    {   // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        // Should we set the function?
        if(SV_ReadByte())
            plat->thinker.function = T_PlatRaise;

        plat->type = (plattype_e) SV_ReadByte();

        sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());

        if(!sector)
            Con_Error("TC_PLAT: bad sector number\n");

        plat->sector = sector;

        plat->speed = FIX2FLT(SV_ReadLong());
        plat->low = (float) SV_ReadShort();
        plat->high = (float) SV_ReadShort();

        plat->wait = SV_ReadLong();
        plat->count = SV_ReadLong();

        plat->status = (plat_e) SV_ReadByte();
        plat->oldstatus = (plat_e) SV_ReadByte();
        plat->crush = (boolean) SV_ReadByte();

        plat->tag = SV_ReadLong();
    }
    else
    {
        // Its in the old format which serialized plat_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_PLAT: bad sector number\n");
        plat->sector = sector;

        plat->speed = FIX2FLT((fixed_t) SV_ReadLong());
        plat->low = FIX2FLT((fixed_t) SV_ReadLong());
        plat->high = FIX2FLT((fixed_t) SV_ReadLong());

        plat->wait = SV_ReadLong();
        plat->count = SV_ReadLong();
        plat->status = SV_ReadLong();
        plat->oldstatus = SV_ReadLong();
        plat->crush = SV_ReadLong();
        plat->tag = SV_ReadLong();
        plat->type = SV_ReadLong();

#if !__JHEXEN__
        if(junk.function)
#endif
            plat->thinker.function = T_PlatRaise;
    }

    P_XSector(plat->sector)->specialdata = plat;
    return true; // Add this thinker.
}

#if __JHEXEN__
static void SV_WriteLight(light_t *th)
{
    SV_WriteByte(TC_LIGHT);

    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteByte((byte) th->type);

    SV_WriteLong(P_ToIndex(th->sector));

    SV_WriteLong((int) (255.0f * th->value1));
    SV_WriteLong((int) (255.0f * th->value2));
    SV_WriteLong(th->tics1);
    SV_WriteLong(th->tics2);
    SV_WriteLong(th->count);
}

static int SV_ReadLight(light_t *th)
{
    sector_t *sector;

    if(saveVersion >= 4)
    {
        /*int ver =*/ SV_ReadByte(); // version byte.

        th->type = (lighttype_t) SV_ReadByte();

        sector = P_ToPtr(DMU_SECTOR, SV_ReadLong());
        if(!sector)
            Con_Error("TC_LIGHT: bad sector number\n");
        th->sector = sector;

        th->value1 = (float) SV_ReadLong() / 255.0f;
        th->value2 = (float) SV_ReadLong() / 255.0f;
        th->tics1 = SV_ReadLong();
        th->tics2 = SV_ReadLong();
        th->count = SV_ReadLong();
    }
    else
    {
        // Its in the old pre V4 format which serialized light_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_LIGHT: bad sector number\n");
        th->sector = sector;

        th->type = (lighttype_t) SV_ReadLong();
        th->value1 = (float) SV_ReadLong() / 255.0f;
        th->value2 = (float) SV_ReadLong() / 255.0f;
        th->tics1 = SV_ReadLong();
        th->tics2 = SV_ReadLong();
        th->count = SV_ReadLong();
    }

    th->thinker.function = T_Light;

    return true; // Add this thinker.
}

static void SV_WritePhase(phase_t *th)
{
    SV_WriteByte(TC_PHASE);

    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(P_ToIndex(th->sector));

    SV_WriteLong(th->index);
    SV_WriteLong((int) (255.0f * th->baseValue));
}

static int SV_ReadPhase(phase_t *th)
{
    sector_t *sector;

    if(saveVersion >= 4)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_PHASE: bad sector number\n");
        th->sector = sector;

        th->index = SV_ReadLong();
        th->baseValue = (float) SV_ReadLong() / 255.0f;
    }
    else
    {
        // Its in the old pre V4 format which serialized phase_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_PHASE: bad sector number\n");
        th->sector = sector;

        th->index = SV_ReadLong();
        th->baseValue = (float) SV_ReadLong() / 255.0f;
    }

    th->thinker.function = T_Phase;

    return true; // Add this thinker.
}

static void SV_WriteScript(acs_t *th)
{
    uint        i;

    SV_WriteByte(TC_INTERPRET_ACS);

    SV_WriteByte(1); // Write a version byte.

    SV_WriteLong(SV_ThingArchiveNum(th->activator));
    SV_WriteLong(th->line ? P_ToIndex(th->line) : -1);
    SV_WriteLong(th->side);
    SV_WriteLong(th->number);
    SV_WriteLong(th->infoIndex);
    SV_WriteLong(th->delayCount);
    for(i = 0; i < ACS_STACK_DEPTH; ++i)
        SV_WriteLong(th->stack[i]);
    SV_WriteLong(th->stackPtr);
    for(i = 0; i < MAX_ACS_SCRIPT_VARS; ++i)
        SV_WriteLong(th->vars[i]);
    SV_WriteLong((int) (th->ip) - (int) ActionCodeBase);
}

static int SV_ReadScript(acs_t *th)
{
    int         temp;
    uint        i;

    if(saveVersion >= 4)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        th->activator = (mobj_t*) SV_ReadLong();
        th->activator = SV_GetArchiveThing((int) th->activator, &th->activator);
        temp = SV_ReadLong();
        if(temp == -1)
            th->line = NULL;
        else
            th->line = P_ToPtr(DMU_LINE, temp);
        th->side = SV_ReadLong();
        th->number = SV_ReadLong();
        th->infoIndex = SV_ReadLong();
        th->delayCount = SV_ReadLong();
        for(i = 0; i < ACS_STACK_DEPTH; ++i)
            th->stack[i] = SV_ReadLong();
        th->stackPtr = SV_ReadLong();
        for(i = 0; i < MAX_ACS_SCRIPT_VARS; ++i)
            th->vars[i] = SV_ReadLong();
        th->ip = (int *) (ActionCodeBase + SV_ReadLong());
    }
    else
    {
        // Its in the old pre V4 format which serialized acs_t
        // Padding at the start (an old thinker_t struct)
        thinker_t   junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
        th->activator = (mobj_t*) SV_ReadLong();
        th->activator = SV_GetArchiveThing((int) th->activator, &th->activator);
        temp = SV_ReadLong();
        if(temp == -1)
            th->line = NULL;
        else
            th->line = P_ToPtr(DMU_LINE, temp);
        th->side = SV_ReadLong();
        th->number = SV_ReadLong();
        th->infoIndex = SV_ReadLong();
        th->delayCount = SV_ReadLong();
        for(i = 0; i < ACS_STACK_DEPTH; ++i)
            th->stack[i] = SV_ReadLong();
        th->stackPtr = SV_ReadLong();
        for(i = 0; i < MAX_ACS_SCRIPT_VARS; ++i)
            th->vars[i] = SV_ReadLong();
        th->ip = (int *) (ActionCodeBase + SV_ReadLong());
    }

    return true; // Add this thinker.
}

static void SV_WriteDoorPoly(polydoor_t *th)
{
    SV_WriteByte(TC_POLY_DOOR);

    SV_WriteByte(1); // Write a version byte.

    SV_WriteByte(th->type);

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(th->polyobj);
    SV_WriteLong(th->speed);
    SV_WriteLong(th->dist);
    SV_WriteLong(th->totalDist);
    SV_WriteLong(th->direction);
    SV_WriteLong(th->xSpeed);
    SV_WriteLong(th->ySpeed);
    SV_WriteLong(th->tics);
    SV_WriteLong(th->waitTics);
    SV_WriteByte(th->close);
}

static int SV_ReadDoorPoly(polydoor_t *th)
{
    if(saveVersion >= 4)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        // Start of used data members.
        th->type = SV_ReadByte();

        th->polyobj = SV_ReadLong();
        th->speed = SV_ReadLong();
        th->dist = SV_ReadLong();
        th->totalDist = SV_ReadLong();
        th->direction = SV_ReadLong();
        th->xSpeed = SV_ReadLong();
        th->ySpeed = SV_ReadLong();
        th->tics = SV_ReadLong();
        th->waitTics = SV_ReadLong();
        th->close = SV_ReadByte();
    }
    else
    {
        // Its in the old pre V4 format which serialized polydoor_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
        th->polyobj = SV_ReadLong();
        th->speed = SV_ReadLong();
        th->dist = SV_ReadLong();
        th->totalDist = SV_ReadLong();
        th->direction = SV_ReadLong();
        th->xSpeed = SV_ReadLong();
        th->ySpeed = SV_ReadLong();
        th->tics = SV_ReadLong();
        th->waitTics = SV_ReadLong();
        th->type = SV_ReadByte();
        th->close = SV_ReadByte();
    }

    th->thinker.function = T_PolyDoor;

    return true; // Add this thinker.
}

static void SV_WriteMovePoly(polyevent_t *th)
{
    SV_WriteByte(TC_MOVE_POLY);

    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(th->polyobj);
    SV_WriteLong(th->speed);
    SV_WriteLong(th->dist);
    SV_WriteLong(th->angle);
    SV_WriteLong(th->xSpeed);
    SV_WriteLong(th->ySpeed);
}

static int SV_ReadMovePoly(polyevent_t *th)
{
    if(saveVersion >= 4)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        // Start of used data members.
        th->polyobj = SV_ReadLong();
        th->speed = SV_ReadLong();
        th->dist = SV_ReadLong();
        th->angle = SV_ReadLong();
        th->xSpeed = SV_ReadLong();
        th->ySpeed = SV_ReadLong();
    }
    else
    {
        // Its in the old pre V4 format which serialized polyevent_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
        th->polyobj = SV_ReadLong();
        th->speed = SV_ReadLong();
        th->dist = SV_ReadLong();
        th->angle = SV_ReadLong();
        th->xSpeed = SV_ReadLong();
        th->ySpeed = SV_ReadLong();
    }

    th->thinker.function = T_MovePoly;

    return true; // Add this thinker.
}

static void SV_WriteRotatePoly(polyevent_t *th)
{
    SV_WriteByte(TC_ROTATE_POLY);

    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(th->polyobj);
    SV_WriteLong(th->speed);
    SV_WriteLong(th->dist);
    SV_WriteLong(th->angle);
    SV_WriteLong(th->xSpeed);
    SV_WriteLong(th->ySpeed);
}

static int SV_ReadRotatePoly(polyevent_t *th)
{
    if(saveVersion >= 4)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        // Start of used data members.
        th->polyobj = SV_ReadLong();
        th->speed = SV_ReadLong();
        th->dist = SV_ReadLong();
        th->angle = SV_ReadLong();
        th->xSpeed = SV_ReadLong();
        th->ySpeed = SV_ReadLong();
    }
    else
    {
        // Its in the old pre V4 format which serialized polyevent_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
        th->polyobj = SV_ReadLong();
        th->speed = SV_ReadLong();
        th->dist = SV_ReadLong();
        th->angle = SV_ReadLong();
        th->xSpeed = SV_ReadLong();
        th->ySpeed = SV_ReadLong();
    }

    th->thinker.function = T_RotatePoly;
    return true; // Add this thinker.
}

static void SV_WritePillar(pillar_t *th)
{
    SV_WriteByte(TC_BUILD_PILLAR);

    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(P_ToIndex(th->sector));

    SV_WriteLong(FLT2FIX(th->ceilingSpeed));
    SV_WriteLong(FLT2FIX(th->floorSpeed));
    SV_WriteLong(FLT2FIX(th->floordest));
    SV_WriteLong(FLT2FIX(th->ceilingdest));
    SV_WriteLong(th->direction);
    SV_WriteLong(th->crush);
}

static int SV_ReadPillar(pillar_t *th)
{
    sector_t *sector;

    if(saveVersion >= 4)
    {
        // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        // Start of used data members.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_BUILD_PILLAR: bad sector number\n");
        th->sector = sector;

        th->ceilingSpeed = FIX2FLT((fixed_t) SV_ReadLong());
        th->floorSpeed = FIX2FLT((fixed_t) SV_ReadLong());
        th->floordest = FIX2FLT((fixed_t) SV_ReadLong());
        th->ceilingdest = FIX2FLT((fixed_t) SV_ReadLong());
        th->direction = SV_ReadLong();
        th->crush = SV_ReadLong();
    }
    else
    {
        // Its in the old pre V4 format which serialized pillar_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_BUILD_PILLAR: bad sector number\n");
        th->sector = sector;

        th->ceilingSpeed = FIX2FLT((fixed_t) SV_ReadLong());
        th->floorSpeed = FIX2FLT((fixed_t) SV_ReadLong());
        th->floordest = FIX2FLT((fixed_t) SV_ReadLong());
        th->ceilingdest = FIX2FLT((fixed_t) SV_ReadLong());
        th->direction = SV_ReadLong();
        th->crush = SV_ReadLong();
    }

    th->thinker.function = T_BuildPillar;

    P_XSector(th->sector)->specialdata = th;
    return true; // Add this thinker.
}

static void SV_WriteFloorWaggle(floorWaggle_t *th)
{
    SV_WriteByte(TC_FLOOR_WAGGLE);

    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(P_ToIndex(th->sector));

    SV_WriteLong(FLT2FIX(th->originalHeight));
    SV_WriteLong(FLT2FIX(th->accumulator));
    SV_WriteLong(FLT2FIX(th->accDelta));
    SV_WriteLong(FLT2FIX(th->targetScale));
    SV_WriteLong(FLT2FIX(th->scale));
    SV_WriteLong(FLT2FIX(th->scaleDelta));
    SV_WriteLong(th->ticker);
    SV_WriteLong(th->state);
}

static int SV_ReadFloorWaggle(floorWaggle_t *th)
{
    sector_t *sector;

    if(saveVersion >= 4)
    {
        /*int ver =*/ SV_ReadByte(); // version byte.

        // Start of used data members.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_FLOOR_WAGGLE: bad sector number\n");
        th->sector = sector;

        th->originalHeight = FIX2FLT((fixed_t) SV_ReadLong());
        th->accumulator = FIX2FLT((fixed_t) SV_ReadLong());
        th->accDelta = FIX2FLT((fixed_t) SV_ReadLong());
        th->targetScale = FIX2FLT((fixed_t) SV_ReadLong());
        th->scale = FIX2FLT((fixed_t) SV_ReadLong());
        th->scaleDelta = FIX2FLT((fixed_t) SV_ReadLong());
        th->ticker = SV_ReadLong();
        th->state = SV_ReadLong();
    }
    else
    {
        // Its in the old pre V4 format which serialized floorWaggle_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_FLOOR_WAGGLE: bad sector number\n");
        th->sector = sector;

        th->originalHeight = FIX2FLT((fixed_t) SV_ReadLong());
        th->accumulator = FIX2FLT((fixed_t) SV_ReadLong());
        th->accDelta = FIX2FLT((fixed_t) SV_ReadLong());
        th->targetScale = FIX2FLT((fixed_t) SV_ReadLong());
        th->scale = FIX2FLT((fixed_t) SV_ReadLong());
        th->scaleDelta = FIX2FLT((fixed_t) SV_ReadLong());
        th->ticker = SV_ReadLong();
        th->state = SV_ReadLong();
    }

    th->thinker.function = T_FloorWaggle;

    P_XSector(th->sector)->specialdata = th;
    return true; // Add this thinker.
}
#endif // __JHEXEN__

#if !__JHEXEN__
static void SV_WriteFlash(lightflash_t* flash)
{
    SV_WriteByte(TC_FLASH);

    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(P_ToIndex(flash->sector));

    SV_WriteLong(flash->count);
    SV_WriteLong((int) (255.0f * flash->maxlight));
    SV_WriteLong((int) (255.0f * flash->minlight));
    SV_WriteLong(flash->maxtime);
    SV_WriteLong(flash->mintime);
}

static int SV_ReadFlash(lightflash_t* flash)
{
    sector_t* sector;

    if(hdr.version >= 5)
    {   // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        // Start of used data members.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_FLASH: bad sector number\n");
        flash->sector = sector;

        flash->count = SV_ReadLong();
        flash->maxlight = (float) SV_ReadLong() / 255.0f;
        flash->minlight = (float) SV_ReadLong() / 255.0f;
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
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_FLASH: bad sector number\n");
        flash->sector = sector;

        flash->count = SV_ReadLong();
        flash->maxlight = (float) SV_ReadLong() / 255.0f;
        flash->minlight = (float) SV_ReadLong() / 255.0f;
        flash->maxtime = SV_ReadLong();
        flash->mintime = SV_ReadLong();
    }

    flash->thinker.function = T_LightFlash;
    return true; // Add this thinker.
}

static void SV_WriteStrobe(strobe_t* strobe)
{
    SV_WriteByte(TC_STROBE);

    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(P_ToIndex(strobe->sector));

    SV_WriteLong(strobe->count);
    SV_WriteLong((int) (255.0f * strobe->maxlight));
    SV_WriteLong((int) (255.0f * strobe->minlight));
    SV_WriteLong(strobe->darktime);
    SV_WriteLong(strobe->brighttime);
}

static int SV_ReadStrobe(strobe_t *strobe)
{
    sector_t* sector;

    if(hdr.version >= 5)
    {   // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_STROBE: bad sector number\n");
        strobe->sector = sector;

        strobe->count = SV_ReadLong();
        strobe->maxlight = (float) SV_ReadLong() / 255.0f;
        strobe->minlight = (float) SV_ReadLong() / 255.0f;
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
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_STROBE: bad sector number\n");
        strobe->sector = sector;

        strobe->count = SV_ReadLong();
        strobe->minlight = (float) SV_ReadLong() / 255.0f;
        strobe->maxlight = (float) SV_ReadLong() / 255.0f;
        strobe->darktime = SV_ReadLong();
        strobe->brighttime = SV_ReadLong();
    }

    strobe->thinker.function = T_StrobeFlash;
    return true; // Add this thinker.
}

static void SV_WriteGlow(glow_t *glow)
{
    SV_WriteByte(TC_GLOW);

    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(P_ToIndex(glow->sector));

    SV_WriteLong((int) (255.0f * glow->maxlight));
    SV_WriteLong((int) (255.0f * glow->minlight));
    SV_WriteLong(glow->direction);
}

static int SV_ReadGlow(glow_t *glow)
{
    sector_t* sector;

    if(hdr.version >= 5)
    {   // Note: the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_GLOW: bad sector number\n");
        glow->sector = sector;

        glow->maxlight = (float) SV_ReadLong() / 255.0f;
        glow->minlight = (float) SV_ReadLong() / 255.0f;
        glow->direction = SV_ReadLong();
    }
    else
    {
        // Its in the old pre V5 format which serialized strobe_t
        // Padding at the start (an old thinker_t struct)
        SV_Read(junkbuffer, (size_t) 16);

        // Start of used data members.
        // A 32bit pointer to sector, serialized.
        sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
        if(!sector)
            Con_Error("TC_GLOW: bad sector number\n");
        glow->sector = sector;

        glow->minlight = (float) SV_ReadLong() / 255.0f;
        glow->maxlight = (float) SV_ReadLong() / 255.0f;
        glow->direction = SV_ReadLong();
    }

    glow->thinker.function = T_Glow;
    return true; // Add this thinker.
}

# if __JDOOM__
static void SV_WriteFlicker(fireflicker_t *flicker)
{
    SV_WriteByte(TC_FLICKER);

    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(P_ToIndex(flicker->sector));

    SV_WriteLong((int) (255.0f * flicker->maxlight));
    SV_WriteLong((int) (255.0f * flicker->minlight));
}

/**
 * T_FireFlicker was added to save games in ver5, therefore we don't have
 * an old format to support.
 */
static int SV_ReadFlicker(fireflicker_t *flicker)
{
    sector_t* sector;
    /*int ver =*/ SV_ReadByte(); // version byte.

    // Note: the thinker class byte has already been read.
    sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
    if(!sector)
        Con_Error("TC_FLICKER: bad sector number\n");
    flicker->sector = sector;

    flicker->maxlight = (float) SV_ReadLong() / 255.0f;
    flicker->minlight = (float) SV_ReadLong() / 255.0f;

    flicker->thinker.function = T_FireFlicker;
    return true; // Add this thinker.
}
# endif

# if __DOOM64TC__
static void SV_WriteBlink(lightblink_t *blink)
{
    SV_WriteByte(TC_BLINK);

    SV_WriteByte(1); // Write a version byte.

    // Note we don't bother to save a byte to tell if the function
    // is present as we ALWAYS add one when loading.

    SV_WriteLong(P_ToIndex(blink->sector));

    SV_WriteLong(blink->count);
    SV_WriteLong((int) (255.0f * blink->maxlight));
    SV_WriteLong((int) (255.0f * blink->minlight));
    SV_WriteLong(blink->maxtime);
    SV_WriteLong(blink->mintime);
}

/**
 * T_LightBlink was added to save games in ver5, therefore we don't have an
 * old format to support
 */
static int SV_ReadBlink(lightblink_t *blink)
{
    sector_t* sector;
    /*int ver =*/ SV_ReadByte(); // version byte.

    // Note: the thinker class byte has already been read.
    sector = P_ToPtr(DMU_SECTOR, (int) SV_ReadLong());
    if(!sector)
        Con_Error("tc_lightblink: bad sector number\n");
    blink->sector = sector;

    blink->count = SV_ReadLong();
    blink->maxlight = (float) SV_ReadLong() / 255.0f;
    blink->minlight = (float) SV_ReadLong() / 255.0f;
    blink->maxtime = SV_ReadLong();
    blink->mintime = SV_ReadLong();

    blink->thinker.function = T_LightBlink;
    return true; // Add this thinker.
}
# endif
#endif // !__JHEXEN__

static void SV_AddThinker(thinkerclass_t tclass, thinker_t* th)
{
    P_AddThinker(&((plat_t *)th)->thinker);

    switch(tclass)
    {
    case TC_CEILING:
        P_AddActiveCeiling((ceiling_t *) th);
        break;
    case TC_PLAT:
        P_AddActivePlat((plat_t *) th);
        break;

    default:
        break;
    }
}

/**
 * Archives the specified thinker.
 *
 * @param thInfo    The thinker info to be used when archiving.
 * @param th        The thinker to be archived.
 */
static void DoArchiveThinker(thinkerinfo_t *thInfo, thinker_t *th)
{
    if(!thInfo || !th)
        return;

    // Only the server saves this class of thinker?
    if((thInfo->flags & TSF_SERVERONLY) && IS_CLIENT)
        return;

    if(thInfo->Write)
        thInfo->Write(th);
}

/**
 * Archives thinkers for both client and server.
 * Clients do not save all data for all thinkers (the server will send
 * it to us anyway so saving it would just bloat client save games).
 *
 * NOTE: Some thinker classes are NEVER saved by clients.
 */
static void P_ArchiveThinkers(boolean savePlayers)
{
#if !__JHEXEN__
    extern ceilinglist_t *activeceilings;
    extern platlist_t *activeplats;
#endif

    thinker_t  *th = 0;
#if !__JHEXEN__
    boolean     found;
#endif

    SV_BeginSegment(ASEG_THINKERS);
#if __JHEXEN__
    SV_WriteLong(thing_archiveSize); // number of mobjs.
#endif

    // Save off the current thinkers
    for(th = thinkercap.next; th != &thinkercap && th; th = th->next)
    {
#if !__JHEXEN__
        if(th->function == INSTASIS) // Special case for thinkers in stasis.
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
                    DoArchiveThinker(thinkerinfo(TC_PLAT), th);
                    found = true;
                }

            for(cl = activeceilings; cl && !found; cl = cl->next)
                if(cl->ceiling == (ceiling_t *) th)      //jff 2/22/98
                {
                    DoArchiveThinker(thinkerinfo(TC_CEILING), th);
                    found = true;
                }
        }
        else
#endif
        {
            if(th->function == P_MobjThinker &&
               ((mobj_t *) th)->player && !savePlayers)
                continue; // Skipping player mobjs

            DoArchiveThinker(infoForThinker(th), th);
        }
    }

    // Add a terminating marker.
    SV_WriteByte(TC_END);
}

/**
 * Un-Archives thinkers for both client and server.
 */
static void P_UnArchiveThinkers(void)
{
    uint        i;
    byte        tClass;
    thinker_t  *th = 0;
    thinkerinfo_t *thInfo = 0;
    boolean     found, knownThinker;
#if __JHEXEN__
    boolean     doSpecials = (saveVersion >= 4);
#else
    boolean     doSpecials = (hdr.version >= 5);
#endif

#if !__JHEXEN__
    if(IS_SERVER)
#endif
        removeAllThinkers();

#if __JHEXEN__
    if(saveVersion < 4)
        AssertSegment(ASEG_MOBJS);
    else
#endif
        AssertSegment(ASEG_THINKERS);

#if __JHEXEN__
    targetPlayerAddrs = NULL;
    SV_InitThingArchive(true, savingPlayers);
#endif

    // Read in saved thinkers.
#if __JHEXEN__
    i = 0;
#endif
    for(;;)
    {
#if __JHEXEN__
        if(doSpecials)
#endif
            tClass = SV_ReadByte();

#if __JHEXEN__
        if(saveVersion < 4)
        {
            if(doSpecials) // Have we started on the specials yet?
            {
                // Versions prior to 4 used a different value to mark
                // the end of the specials data and the thinker class ids
                // are differrent, so we need to manipulate the thinker
                // class identifier value.
                if(tClass != TC_END)
                    tClass += 2;
            }
            else
                tClass = TC_MOBJ;

            if(tClass == TC_MOBJ && i == thing_archiveSize)
            {
                AssertSegment(ASEG_THINKERS);
                // We have reached the begining of the "specials" block.
                doSpecials = true;
                continue;
            }
        }
#else
        if(hdr.version < 5)
        {
            if(doSpecials) // Have we started on the specials yet?
            {
                // Versions prior to 5 used a different value to mark
                // the end of the specials data so we need to manipulate
                // the thinker class identifier value.
                if(tClass == PRE_VER5_END_SPECIALS)
                    tClass = TC_END;
                else
                    tClass += 3;
            }
            else if(tClass == TC_END)
            {
                // We have reached the begining of the "specials" block.
                doSpecials = true;
                continue;
            }
        }
#endif
        if(tClass == TC_END)
            break; // End of the list.

        found = knownThinker = false;
        thInfo = thinkerInfo;
        while(thInfo->thinkclass != TC_NULL && !found)
        {
            if(thInfo->thinkclass == tClass)
            {
                found = true;

                // Not for us? (it shouldn't be here anyway!).
                if(!((thInfo->flags & TSF_SERVERONLY) && IS_CLIENT))
                {
                    th = Z_Calloc(thInfo->size,
                                  ((thInfo->flags & TSF_SPECIAL)? PU_LEVSPEC : PU_LEVEL),
                                  0);
                    knownThinker = thInfo->Read(th);
                }
            }
            if(!found)
                thInfo++;
        }
#if __JHEXEN__
        if(tClass == TC_MOBJ)
            i++;
#endif
        if(!found)
            Con_Error("P_UnarchiveThinkers: Unknown tClass %i in savegame",
                      tClass);

        if(knownThinker)
            SV_AddThinker(tClass, th);
    }

    // Update references to things.
#if __JHEXEN__
    {
        mobj_t* mo;

        for(th = thinkercap.next; th != &thinkercap; th = th->next)
        {
            if(th->function != P_MobjThinker)
                continue;

            mo = (mobj_t *) th;
            mo->target = SV_GetArchiveThing((int) mo->target, &mo->target);

            switch(mo->type)
            {
            // Just tracer
            case MT_BISH_FX:
            case MT_HOLY_FX:
            case MT_DRAGON:
            case MT_THRUSTFLOOR_UP:
            case MT_THRUSTFLOOR_DOWN:
            case MT_MINOTAUR:
            case MT_SORCFX1:
                if(saveVersion >= 3)
                {
                    mo->tracer = SV_GetArchiveThing((int) mo->tracer, &mo->tracer);
                }
                else
                {
                    mo->tracer = SV_GetArchiveThing(mo->special1, &mo->tracer);
                    mo->special1 = 0;
                }
                break;

            // Just special2
            case MT_LIGHTNING_FLOOR:
            case MT_LIGHTNING_ZAP:
                mo->special2 = (int) SV_GetArchiveThing(mo->special2, &mo->special2);
                break;

            // Both tracer and special2
            case MT_HOLY_TAIL:
            case MT_LIGHTNING_CEILING:
                if(saveVersion >= 3)
                {
                    mo->tracer = SV_GetArchiveThing((int) mo->tracer, &mo->tracer);
                }
                else
                {
                    mo->tracer = SV_GetArchiveThing(mo->special1, &mo->tracer);
                    mo->special1 = 0;
                }
                mo->special2 = (int) SV_GetArchiveThing(mo->special2, &mo->special2);
                break;

            default:
                break;
            }
        }
    }
#else
    if(IS_SERVER)
    {
        mobj_t* mo;

        for(th = thinkercap.next; th != &thinkercap; th = th->next)
        {
            if(th->function != P_MobjThinker)
                continue;

            mo = (mobj_t *) th;

            mo->target = SV_GetArchiveThing((int) mo->target, &mo->target);
            mo->onmobj = SV_GetArchiveThing((int) mo->onmobj, &mo->onmobj);
# if __JDOOM__
            mo->tracer = SV_GetArchiveThing((int) mo->tracer, &mo->tracer);
# endif
# if __JHERETIC__
            mo->generator = SV_GetArchiveThing((int) mo->generator, &mo->generator);
# endif
        }

        for(i = 0; i < numlines; ++i)
        {
            xline_t *xline = P_XLine(P_ToPtr(DMU_LINE, i));
            if(xline->xg)
                xline->xg->activator =
                    SV_GetArchiveThing((int) xline->xg->activator,
                                       &xline->xg->activator);
        }
    }
#endif

#if __JHEXEN__
    P_CreateTIDList();
    P_InitCreatureCorpseQueue(true);    // true = scan for corpses
#endif
}

#ifdef __JDOOM__
static void P_ArchiveBrain(void)
{
    int     i;

    SV_WriteByte(numbraintargets);
    SV_WriteByte(brain.targeton);
    // Write the mobj references using the mobj archive.
    for(i = 0; i < numbraintargets; ++i)
        SV_WriteShort(SV_ThingArchiveNum(braintargets[i]));
}

static void P_UnArchiveBrain(void)
{
    int     i;

    if(hdr.version < 3)
        return;    // No brain data before version 3.

    numbraintargets = SV_ReadByte();
    brain.targeton = SV_ReadByte();
    for(i = 0; i < numbraintargets; ++i)
    {
        braintargets[i] = (mobj_t*) (int) SV_ReadShort();
        braintargets[i] = SV_GetArchiveThing((int) braintargets[i], NULL);
    }

    if(gamemode == commercial)
        P_SpawnBrainTargets();
}
#endif

#if !__JHEXEN__
static void P_ArchiveSoundTargets(void)
{
    uint        i;
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
    uint        i;
    uint        secid;
    uint        numsoundtargets;
    xsector_t  *xsec;

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

        xsec = P_XSector(P_ToPtr(DMU_SECTOR, secid));
        xsec->soundtarget = (mobj_t*) (int) SV_ReadShort();
            SV_GetArchiveThing((int) xsec->soundtarget, &xsec->soundtarget);
    }
}
#endif

#if __JHEXEN__
static void P_ArchiveSounds(void)
{
    seqnode_t  *node;
    sector_t   *sec;
    int         difference;
    uint        i;

    // Save the sound sequences
    SV_BeginSegment(ASEG_SOUNDS);
    SV_WriteLong(ActiveSequences);
    for(node = SequenceListHead; node; node = node->next)
    {
        SV_WriteByte(1); // write a version byte.

        SV_WriteLong(node->sequence);
        SV_WriteLong(node->delayTics);
        SV_WriteLong(node->volume);
        SV_WriteLong(SN_GetSequenceOffset(node->sequence, node->sequencePtr));
        SV_WriteLong(node->currentSoundID);
        for(i = 0; i < numpolyobjs; ++i)
        {
            if(node->mobj == P_GetPtr(DMU_POLYOBJ, i, DMU_START_SPOT))
            {
                break;
            }
        }

        if(i == numpolyobjs)
        {   // Sound is attached to a sector, not a polyobj
            sec = P_GetPtrp(R_PointInSubsector(node->mobj->pos[VX], node->mobj->pos[VY]),
                            DMU_SECTOR);
            difference = P_ToIndex(sec);
            SV_WriteLong(0);   // 0 -- sector sound origin
        }
        else
        {
            SV_WriteLong(1);   // 1 -- polyobj sound origin
            difference = i;
        }
        SV_WriteLong(difference);
    }
}

static void P_UnArchiveSounds(void)
{
    int         i, ver;
    int         numSequences;
    int         sequence;
    int         delayTics;
    int         volume;
    int         seqOffset;
    int         soundID;
    int         polySnd;
    int         secNum;
    mobj_t     *sndMobj;

    AssertSegment(ASEG_SOUNDS);

    // Reload and restart all sound sequences
    numSequences = SV_ReadLong();
    i = 0;
    while(i < numSequences)
    {
        if(saveVersion >= 3)
            ver = SV_ReadByte();

        sequence = SV_ReadLong();
        delayTics = SV_ReadLong();
        volume = SV_ReadLong();
        seqOffset = SV_ReadLong();

        soundID = SV_ReadLong();
        polySnd = SV_ReadLong();
        secNum = SV_ReadLong();
        if(!polySnd)
        {
            sndMobj = P_GetPtr(DMU_SECTOR, secNum, DMU_SOUND_ORIGIN);
        }
        else
        {
            sndMobj = P_GetPtr(DMU_POLYOBJ, secNum, DMU_START_SPOT);
        }
        SN_StartSequence(sndMobj, sequence);
        SN_ChangeNodeData(i, seqOffset, delayTics, volume, soundID);
        i++;
    }
}

static void P_ArchiveScripts(void)
{
    int         i;

    SV_BeginSegment(ASEG_SCRIPTS);
    for(i = 0; i < ACScriptCount; ++i)
    {
        SV_WriteShort(ACSInfo[i].state);
        SV_WriteShort(ACSInfo[i].waitValue);
    }
    SV_Write(MapVars, sizeof(MapVars));
}

static void P_UnArchiveScripts(void)
{
    int         i;

    AssertSegment(ASEG_SCRIPTS);
    for(i = 0; i < ACScriptCount; ++i)
    {
        ACSInfo[i].state = SV_ReadShort();
        ACSInfo[i].waitValue = SV_ReadShort();
    }
    memcpy(MapVars, saveptr.b, sizeof(MapVars));
    saveptr.b += sizeof(MapVars);
}

static void P_ArchiveMisc(void)
{
    int         ix;

    SV_BeginSegment(ASEG_MISC);
    for(ix = 0; ix < MAXPLAYERS; ++ix)
    {
        SV_WriteLong(localQuakeHappening[ix]);
    }
}

static void P_UnArchiveMisc(void)
{
    int         ix;

    AssertSegment(ASEG_MISC);
    for(ix = 0; ix < MAXPLAYERS; ++ix)
    {
        localQuakeHappening[ix] = SV_ReadLong();
    }
}
#endif

static void P_ArchiveMap(boolean savePlayers)
{
    // Place a header marker
    SV_BeginSegment(ASEG_MAP_HEADER2);

#if __JHEXEN__
    savingPlayers = savePlayers;

    // Write a version byte
    SV_WriteByte(MY_SAVE_VERSION);

    // Write the level timer
    SV_WriteLong(leveltime);
#else
    // Clear the sound target count (determined while saving sectors).
    numSoundTargets = 0;
#endif

    SV_InitTextureArchives();

    P_ArchiveWorld();
    P_ArchiveThinkers(savePlayers);

#if __JHEXEN__
    P_ArchiveScripts();
    P_ArchiveSounds();
    P_ArchiveMisc();
#else
    if(IS_SERVER)
    {
# ifdef __JDOOM__
        // Doom saves the brain data, too. (It's a part of the world.)
        P_ArchiveBrain();
# endif
        // Save the sound target data (prevents bug where monsters who have
        // been alerted go back to sleep when loading a save game).
        P_ArchiveSoundTargets();
    }
#endif

    // Place a termination marker
    SV_BeginSegment(ASEG_END);
}

static void P_UnArchiveMap(void)
{
#if __JHEXEN__
    int         segType = SV_ReadLong();

    // Determine the map version.
    if(segType == ASEG_MAP_HEADER2)
    {
        saveVersion = SV_ReadByte();
    }
    else if(segType == ASEG_MAP_HEADER)
    {
        saveVersion = 2;
    }
    else
    {
        Con_Error("Corrupt save game: Segment [%d] failed alignment check",
                  ASEG_MAP_HEADER);
    }

    // Read the level timer
    leveltime = SV_ReadLong();
#endif

    P_UnArchiveWorld();
    P_UnArchiveThinkers();

#if __JHEXEN__
    P_UnArchiveScripts();
    P_UnArchiveSounds();
    P_UnArchiveMisc();
#else
    if(IS_SERVER)
    {
#ifdef __JDOOM__
        // Doom saves the brain data, too. (It's a part of the world.)
        P_UnArchiveBrain();
#endif

        // Read the sound target data (prevents bug where monsters who have
        // been alerted go back to sleep when loading a save game).
        P_UnArchiveSoundTargets();
    }
#endif

    AssertSegment(ASEG_END);
}

int SV_GetSaveDescription(char *filename, char *str)
{
#if __JHEXEN__
    LZFILE     *fp;
    char        name[256];
    char        versionText[HXS_VERSION_TEXT_LENGTH];
    boolean     found = false;

    strncpy(name, filename, sizeof(name));
    M_TranslatePath(name, name);
    fp = lzOpen(name, "rp");
    if(fp)
    {
        lzRead(str, SAVESTRINGSIZE, fp);
        lzRead(versionText, HXS_VERSION_TEXT_LENGTH, fp);
        lzClose(fp);
        if(!strncmp(versionText, HXS_VERSION_TEXT, 8))
        {
            saveVersion = atoi(&versionText[8]);
            if(saveVersion <= MY_SAVE_VERSION)
                found = true;
        }
    }
    return found;
#else
    savefile = lzOpen(filename, "rp");
    if(!savefile)
    {
# if __DOOM64TC__ || __WOLFTC__
        // we don't support the original game's save format (for obvious reasons).
        return false;
# else
        // It might still be a v19 savegame.
        savefile = lzOpen(filename, "r");
        if(!savefile)
            return false;       // It just doesn't exist.
        lzRead(str, SAVESTRINGSIZE, savefile);
        str[SAVESTRINGSIZE - 1] = 0;
        lzClose(savefile);
        return true;
# endif
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
#endif
}

/**
 * Initialize the savegame directories.
 * If the directories do not exist, they are created.
 */
void SV_Init(void)
{
    if(ArgCheckWith("-savedir", 1))
    {
        strcpy(savePath, ArgNext());
        // Add a trailing backslash is necessary.
        if(savePath[strlen(savePath) - 1] != '\\')
            strcat(savePath, "\\");
    }
    else
    {
        // Use the default path.
#if __JHEXEN__
        sprintf(savePath, "hexndata\\%s\\", (char *) G_GetVariable(DD_GAME_MODE));
#else
        sprintf(savePath, "savegame\\%s\\", (char *) G_GetVariable(DD_GAME_MODE));
#endif
    }

    // Build the client save path.
    strcpy(clientSavePath, savePath);
    strcat(clientSavePath, "client\\");

    // Check that the save paths exist.
    M_CheckPath(savePath);
    M_CheckPath(clientSavePath);
#if !__JHEXEN__
    M_TranslatePath(savePath, savePath);
    M_TranslatePath(clientSavePath, clientSavePath);
#endif
}

void SV_SaveGameFile(int slot, char *str)
{
    sprintf(str, "%s" SAVEGAMENAME "%i." SAVEGAMEEXTENSION, savePath, slot);
}

void SV_ClientSaveGameFile(unsigned int game_id, char *str)
{
    sprintf(str, "%s" CLIENTSAVEGAMENAME "%08X." SAVEGAMEEXTENSION,
            clientSavePath, game_id);
}

#if __JHEXEN__
boolean SV_SaveGame(int slot, char *description)
#else
boolean SV_SaveGame(char *filename, char *description)
#endif
{
#if __JHEXEN__
    char        fileName[256];
    char        versionText[HXS_VERSION_TEXT_LENGTH];
#else
    int     i;
#endif

    playerHeaderOK = false; // Uninitialized.

#if __JHEXEN__
    // Open the output file
    sprintf(fileName, "%shex6.hxs", savePath);
    M_TranslatePath(fileName, fileName);
    OpenStreamOut(fileName);
#else
    savefile = lzOpen(filename, "wp");
    if(!savefile)
    {
        Con_Message("P_SaveGame: couldn't open \"%s\" for writing.\n",
                    filename);
        return false;           // No success.
    }
#endif

#if __JHEXEN__
    // Write game save description
    SV_Write(description, SAVESTRINGSIZE);

    // Write version info
    memset(versionText, 0, HXS_VERSION_TEXT_LENGTH);
    sprintf(versionText, HXS_VERSION_TEXT"%i", MY_SAVE_VERSION);
    SV_Write(versionText, HXS_VERSION_TEXT_LENGTH);

    // Place a header marker
    SV_BeginSegment(ASEG_GAME_HEADER);

    // Write current map and difficulty
    SV_WriteByte(gamemap);
    SV_WriteByte(gameskill);
    SV_WriteByte(deathmatch);
    SV_WriteByte(nomonsters);
    SV_WriteByte(randomclass);

    // Write global script info
    SV_Write(WorldVars, sizeof(WorldVars));
    SV_Write(ACSStore, sizeof(ACSStore));
#else
    // Write the header.
    hdr.magic = MY_SAVE_MAGIC;
    hdr.version = MY_SAVE_VERSION;
# if __JDOOM__
    hdr.gamemode = gamemode;
# elif __JHERETIC__
    hdr.gamemode = 0;
# endif

    strncpy(hdr.description, description, SAVESTRINGSIZE);
    hdr.description[SAVESTRINGSIZE - 1] = 0;
    hdr.skill = gameskill;
# if __JDOOM__
    if(fastparm)
        hdr.skill |= 0x80;      // Set high byte.
# endif
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
#endif

    // Set the mobj archive numbers
    SV_InitThingArchive(false, true);
#if !__JHEXEN__
    SV_WriteLong(thing_archiveSize);
#endif

    P_ArchivePlayerHeader();
    P_ArchivePlayers();

    // Place a termination marker
    SV_BeginSegment(ASEG_END);

#if __JHEXEN__
    // Close the output file (maps are saved into a seperate file).
    CloseStreamOut();
#endif

    // Save out the current map
#if __JHEXEN__
    {
        char        fileName[100];

        // Open the output file
        sprintf(fileName, "%shex6%02d.hxs", savePath, gamemap);
        M_TranslatePath(fileName, fileName);
        OpenStreamOut(fileName);

        P_ArchiveMap(true);         // true = save player info

        // Close the output file
        CloseStreamOut();
    }
#else
    P_ArchiveMap(true);
#endif

#if!__JHEXEN__
    // To be absolutely sure...
    SV_WriteByte(CONSISTENCY);

    SV_FreeThingArchive();
    lzClose(savefile);
#endif

#if __JHEXEN__
    // Clear all save files at destination slot.
    ClearSaveSlot(slot);

    // Copy base slot to destination slot
    CopySaveSlot(BASE_SLOT, slot);
#endif

    return true; // Success!
}

#if __JHEXEN__
static boolean readSaveHeader(void)
#else
static boolean readSaveHeader(saveheader_t *hdr, LZFILE *savefile)
#endif
{
#if __JHEXEN__
    // Set the save pointer and skip the description field
    saveptr.b = saveBuffer + SAVESTRINGSIZE;

    if(strncmp(saveptr.b, HXS_VERSION_TEXT, 8))
    {
        Con_Message("SV_LoadGame: Bad magic.\n");
        return false;
    }

    saveVersion = atoi(saveptr.b + 8);

    // Check for unsupported versions.
    if(saveVersion > MY_SAVE_VERSION)
    {
        return false; // A future version
    }
    // We are incompatible with ver3 saves. Due to an invalid test
    // used to determine present sidedefs, the ver3 format's sides
    // included chunks of junk data.
    if(saveVersion == 3)
        return false;

    saveptr.b += HXS_VERSION_TEXT_LENGTH;

    AssertSegment(ASEG_GAME_HEADER);

    gameepisode = 1;
    gamemap = SV_ReadByte();
    gameskill = SV_ReadByte();
    deathmatch = SV_ReadByte();
    nomonsters = SV_ReadByte();
    randomclass = SV_ReadByte();

#else
    lzRead(hdr, sizeof(*hdr), savefile);

    if(hdr->magic != MY_SAVE_MAGIC)
    {
        Con_Message("SV_LoadGame: Bad magic.\n");
        return false;
    }

    // Check for unsupported versions.
    if(hdr->version > MY_SAVE_VERSION)
    {
        return false; // A future version.
    }

# ifdef __JDOOM__
    if(hdr->gamemode != gamemode && !ArgExists("-nosavecheck"))
    {
        Con_Message("SV_LoadGame: savegame not from gamemode %i.\n",
                    gamemode);
        return false;
    }
# endif

    gameskill = hdr->skill & 0x7f;
# ifdef __JDOOM__
    fastparm = (hdr->skill & 0x80) != 0;
# endif
    gameepisode = hdr->episode;
    gamemap = hdr->map;
    deathmatch = hdr->deathmatch;
    nomonsters = hdr->nomonsters;
    respawnmonsters = hdr->respawn;
#endif

    return true; // Read was OK.
}

static boolean SV_LoadGame2(void)
{
    int         i;
    char        buf[80];
    boolean     loaded[MAXPLAYERS], infile[MAXPLAYERS];
#if __JHEXEN__
    int         k;
    player_t    playerBackup[MAXPLAYERS];
#endif

    // Read the header.
#if __JHEXEN__
    if(!readSaveHeader())
#else
    if(!readSaveHeader(&hdr, savefile))
#endif
        return false; // Something went wrong.

    // Read global save data not part of the game metadata.
#if __JHEXEN__
    // Read global script info
    memcpy(WorldVars, saveptr.b, sizeof(WorldVars));
    saveptr.b += sizeof(WorldVars);
    memcpy(ACSStore, saveptr.b, sizeof(ACSStore));
    saveptr.b += sizeof(ACSStore);
#endif

    // Allocate a small junk buffer.
    // (Data from old save versions is read into here).
    junkbuffer = malloc(sizeof(byte) * 64);

#if !__JHEXEN__
    // Load the level.
    G_InitNew(gameskill, gameepisode, gamemap);

    // Set the time.
    leveltime = hdr.leveltime;

    SV_InitThingArchive(true, true);
#endif

    P_UnArchivePlayerHeader();
    // Read the player structures
    AssertSegment(ASEG_PLAYERS);

    // We don't have the right to say which players are in the game. The
    // players that already are will continue to be. If the data for a given
    // player is not in the savegame file, he will be notified. The data for
    // players who were saved but are not currently in the game will be
    // discarded.
#if !__JHEXEN__
    for(i = 0; i < MAXPLAYERS; ++i)
        infile[i] = hdr.players[i];
#else
    for(i = 0; i < MAXPLAYERS; ++i)
        infile[i] = SV_ReadByte();
#endif

    memset(loaded, 0, sizeof(loaded));
    P_UnArchivePlayers(infile, loaded);
    AssertSegment(ASEG_END);

#if __JHEXEN__
    Z_Free(saveBuffer);

    // Save player structs
    memcpy(playerBackup, players, sizeof(playerBackup));
#endif

    // Load the current map
#if __JHEXEN__
    SV_HxLoadMap();
#else
    // Load the current map
    SV_DMLoadMap();
#endif

#if !__JHEXEN__
    // Check consistency.
    if(SV_ReadByte() != CONSISTENCY)
        Con_Error("SV_LoadGame: Bad savegame (consistency test failed!)\n");

    // We're done.
    SV_FreeThingArchive();
    lzClose(savefile);
#endif

#if __JHEXEN__
    // Don't need the player mobj relocation info for load game
    SV_FreeTargetPlayerList();

    // Restore player structs
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        mobj_t *mo = players[i].plr->mo;

        memcpy(&players[i], &playerBackup[i], sizeof(player_t));
        players[i].plr->mo = mo;
        players[i].readyArtifact = players[i].inventory[players[i].inv_ptr].type;

        P_InventoryResetCursor(&players[i]);
    }
#endif

#if !__JHEXEN__
    // The activator mobjs must be set.
    XL_UpdateActivators();
#endif

    // Notify the players that weren't in the savegame.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        boolean notLoaded = false;

#if __JHEXEN__
        if(players[i].plr->ingame)
        {
            // Try to find a saved player that corresponds this one.
            for(k = 0; k < MAXPLAYERS; ++k)
                if(saveToRealPlayerNum[k] == i)
                    break;
            if(k < MAXPLAYERS)
                continue;           // Found; don't bother this player.

            players[i].playerstate = PST_REBORN;

            if(!i)
            {
                // If the consoleplayer isn't in the save, it must be some
                // other player's file?
                P_SetMessage(players, GET_TXT(TXT_LOADMISSING), false);
            }
            else
            {
                NetSv_SendMessage(i, GET_TXT(TXT_LOADMISSING));
                notLoaded = true;
            }
        }
#else
        if(!loaded[i] && players[i].plr->ingame)
        {
            if(!i)
            {
                P_SetMessage(players, GET_TXT(TXT_LOADMISSING), false);
            }
            else
            {
                NetSv_SendMessage(i, GET_TXT(TXT_LOADMISSING));
            }
            notLoaded = true;
        }
#endif

        if(notLoaded)
        {
            // Kick this player out, he doesn't belong here.
            sprintf(buf, "kick %i", i);
            DD_Execute(buf, false);
        }
    }

#if !__JHEXEN__
    // In netgames, the server tells the clients about this.
    NetSv_LoadGame(hdr.gameid);
#endif

    return true; // Success!
}

#if __JHEXEN__
boolean SV_LoadGame(int slot)
#else
boolean SV_LoadGame(char *filename)
#endif
{
#if __JHEXEN__
    char        fileName[200];

    // Copy all needed save files to the base slot
    if(slot != BASE_SLOT)
    {
        ClearSaveSlot(BASE_SLOT);
        CopySaveSlot(slot, BASE_SLOT);
    }

    // Create the name
    sprintf(fileName, "%shex6.hxs", savePath);
    M_TranslatePath(fileName, fileName);

    // Load the file
    M_ReadFile(fileName, &saveBuffer);
#else
    // Make sure an opening briefing is not shown.
    // (G_InitNew --> G_DoLoadLevel)
    brief_disabled = true;

    savefile = lzOpen(filename, "rp");
    if(!savefile)
    {
# if __DOOM64TC__ || __WOLFTC__
        // we don't support the original game's save format (for obvious reasons).
        return false;
# else
#  if __JDOOM__
        // It might still be a v19 savegame.
        SV_v19_LoadGame(filename);
#  elif __JHERETIC__
        SV_v13_LoadGame(filename);
#  endif
# endif
        return true;
    }
#endif

    playerHeaderOK = false; // Uninitialized.

    return SV_LoadGame2();
}

/**
 * Saves a snapshot of the world, a still image.
 * No data of movement is included (server sends it).
 */
void SV_SaveClient(unsigned int gameid)
{
#if !__JHEXEN__ // unsupported in jHexen
    char    name[200];
    player_t *pl = players + consoleplayer;
    mobj_t *mo = players[consoleplayer].plr->mo;

    if(!IS_CLIENT || !mo)
        return;

    playerHeaderOK = false; // Uninitialized.

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
    SV_WriteLong(FLT2FIX(mo->floorz));
    SV_WriteLong(FLT2FIX(mo->ceilingz));
    SV_WriteLong(mo->angle); /* $unifiedangles */
    SV_WriteFloat(pl->plr->lookdir); /* $unifiedangles */
    P_ArchivePlayerHeader();
    SV_WritePlayer(consoleplayer);

    P_ArchiveMap(true);

    lzClose(savefile);
    free(junkbuffer);
#endif
}

void SV_LoadClient(unsigned int gameid)
{
#if !__JHEXEN__ // unsupported in jHexen
    char    name[200];
    player_t *cpl = players + consoleplayer;
    mobj_t *mo = cpl->plr->mo;

    if(!IS_CLIENT || !mo)
        return;

    playerHeaderOK = false; // Uninitialized.

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
    mo->floorz = FIX2FLT(SV_ReadLong());
    mo->ceilingz = FIX2FLT(SV_ReadLong());
    mo->angle = SV_ReadLong(); /* $unifiedangles */
    cpl->plr->lookdir = SV_ReadFloat(); /* $unifiedangles */
    P_UnArchivePlayerHeader();
    SV_ReadPlayer(cpl);

    P_UnArchiveMap();

    lzClose(savefile);
    free(junkbuffer);

    // The activator mobjs must be set.
    XL_UpdateActivators();
#endif
}

#if !__JHEXEN__
static void SV_DMLoadMap(void)
{
    P_UnArchiveMap();

    // Spawn particle generators, fix HOMS etc, etc...
    R_SetupLevel(DDSLM_AFTER_LOADING, 0);
}
#endif

#if __JHEXEN__
static void SV_HxLoadMap(void)
{
    char        fileName[100];

#ifdef _DEBUG
    Con_Printf("SV_HxLoadMap: Begin, G_InitNew...\n");
#endif

    // We don't want to see a briefing if we're loading a save game.
    brief_disabled = true;

    // Load a base level
    G_InitNew(gameskill, gameepisode, gamemap);

    // Create the name
    sprintf(fileName, "%shex6%02d.hxs", savePath, gamemap);
    M_TranslatePath(fileName, fileName);

#ifdef _DEBUG
    Con_Printf("SV_HxLoadMap: Reading %s\n", fileName);
#endif

    // Load the file
    M_ReadFile(fileName, &saveBuffer);
    saveptr.b = saveBuffer;

    P_UnArchiveMap();

    // Free mobj list and save buffer
    SV_FreeThingArchive();
    Z_Free(saveBuffer);

    // Spawn particle generators, fix HOMS etc, etc...
    R_SetupLevel(DDSLM_AFTER_LOADING, 0);
}

void SV_MapTeleport(int map, int position)
{
    int         i;
    int         j;
    char        fileName[100];
    player_t    playerBackup[MAXPLAYERS];
    mobj_t     *targetPlayerMobj;
    boolean     rClass;
    boolean     playerWasReborn;
    boolean     oldWeaponowned[NUM_WEAPON_TYPES];
    int         oldKeys = 0;
    int         oldPieces = 0;
    int         bestWeapon;

    playerHeaderOK = false; // Uninitialized.

    if(!deathmatch)
    {
        if(P_GetMapCluster(gamemap) == P_GetMapCluster(map))
        {   // Same cluster - save map without saving player mobjs
            char        fileName[100];

            // Set the mobj archive numbers
            SV_InitThingArchive(false, false);

            // Open the output file
            sprintf(fileName, "%shex6%02d.hxs", savePath, gamemap);
            M_TranslatePath(fileName, fileName);
            OpenStreamOut(fileName);

            P_ArchiveMap(false);

            // Close the output file
            CloseStreamOut();
        }
        else
        {   // Entering new cluster - clear base slot
            ClearSaveSlot(BASE_SLOT);
        }
    }

    // Store player structs for later
    rClass = randomclass;
    randomclass = false;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        memcpy(&playerBackup[i], &players[i], sizeof(player_t));
    }

    // Only SV_HxLoadMap() uses targetPlayerAddrs, so it's NULLed here
    // for the following check (player mobj redirection)
    targetPlayerAddrs = NULL;

    gamemap = map;
    sprintf(fileName, "%shex6%02d.hxs", savePath, gamemap);
    M_TranslatePath(fileName, fileName);
    if(!deathmatch && ExistingFile(fileName))
    {                           // Unarchive map
        SV_HxLoadMap();
        brief_disabled = true;
    }
    else
    {                           // New map
        G_InitNew(gameskill, gameepisode, gamemap);

        // Destroy all freshly spawned players
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            if(players[i].plr->ingame)
            {
                P_RemoveMobj(players[i].plr->mo);
            }
        }
    }

    // Restore player structs
    targetPlayerMobj = NULL;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->ingame)
        {
            continue;
        }
        memcpy(&players[i], &playerBackup[i], sizeof(player_t));
        HUMsg_ClearMessages(&players[i]);
        players[i].attacker = NULL;
        players[i].poisoner = NULL;

        if(IS_NETGAME || deathmatch)
        {
            if(players[i].playerstate == PST_DEAD)
            {   // In a network game, force all players to be alive
                players[i].playerstate = PST_REBORN;
            }
            if(!deathmatch)
            {   // Cooperative net-play, retain keys and weapons
                oldKeys = players[i].keys;
                oldPieces = players[i].pieces;
                for(j = 0; j < NUM_WEAPON_TYPES; j++)
                {
                    oldWeaponowned[j] = players[i].weaponowned[j];
                }
            }
        }
        playerWasReborn = (players[i].playerstate == PST_REBORN);
        if(deathmatch)
        {
            memset(players[i].frags, 0, sizeof(players[i].frags));
            players[i].plr->mo = NULL;
            G_DeathMatchSpawnPlayer(i);
        }
        else
        {
            P_SpawnPlayer(P_GetPlayerStart(position, i), i);
        }

        if(playerWasReborn && IS_NETGAME && !deathmatch)
        {   // Restore keys and weapons when reborn in co-op
            players[i].keys = oldKeys;
            players[i].pieces = oldPieces;
            for(bestWeapon = 0, j = 0; j < NUM_WEAPON_TYPES; ++j)
            {
                if(oldWeaponowned[j])
                {
                    bestWeapon = j;
                    players[i].weaponowned[j] = true;
                }
            }
            players[i].ammo[AT_BLUEMANA] = 25;
            players[i].ammo[AT_GREENMANA] = 25;
            if(bestWeapon)
            {                   // Bring up the best weapon
                players[i].pendingweapon = bestWeapon;
            }
        }

        if(targetPlayerMobj == NULL)
        {                       // The poor sap
            targetPlayerMobj = players[i].plr->mo;
        }
    }
    randomclass = rClass;

    //// \fixme Redirect anything targeting a player mobj
    //// FIXME! This only supports single player games!!
    if(targetPlayerAddrs)
    {
        targetplraddress_t *p;

        p = targetPlayerAddrs;
        while(p != NULL)
        {
            *(p->address) = targetPlayerMobj;
            p = p->next;
        }
        SV_FreeTargetPlayerList();

        /* DJS - When XG is available in jHexen, call this after updating
        target player references (after a load).
        // The activator mobjs must be set.
        XL_UpdateActivators();
        */
    }

    // Destroy all things touching players
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->ingame)
        {
            P_TeleportMove(players[i].plr->mo, players[i].plr->mo->pos[VX],
                           players[i].plr->mo->pos[VY], true);
        }
    }

    // Launch waiting scripts
    if(!deathmatch)
    {
        P_CheckACSStore();
    }

    // For single play, save immediately into the reborn slot
    if(!IS_NETGAME && !deathmatch)
    {
        SV_SaveGame(REBORN_SLOT, REBORN_DESCRIPTION);
    }
}
#endif
