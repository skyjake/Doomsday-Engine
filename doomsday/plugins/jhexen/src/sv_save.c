
//**************************************************************************
//**
//** sv_save.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile$
//** $Revision$
//** $Date$
//** $Author$
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <lzss.h>

#include "jhexen.h"
#include "p_svtexarc.h"
#include "f_infine.h"

#include "p_mapsetup.h"

// MACROS ------------------------------------------------------------------

#define DEFAULT_SAVEPATH        "hexndata\\"

#define CLIENTSAVEGAMENAME      "HexenCl"

#define DBG(x)                  //x

#define MAX_TARGET_PLAYERS 512
#define MOBJ_NULL -1
#define MOBJ_XX_PLAYER -2
#define GET_BYTE (*SavePtr.b++)
#define GET_WORD (SHORT(*SavePtr.w++))
#define GET_LONG (LONG(*SavePtr.l++))
#define GET_FLOAT (FLOAT(*SavePtr.f++))
#define GET_DATA(x,y) (memcpy((x), SavePtr.b, (y)), SavePtr.b += (y))
#define GET_BUFFER(x) GET_DATA(x, sizeof(x))
#define MAX_MAPS 99
#define BASE_SLOT 6
#define REBORN_SLOT 7
#define REBORN_DESCRIPTION "TEMP GAME"
#define MAX_THINKER_SIZE 256
#define INVALID_PLAYER ((player_t*) -1 )

// TYPES -------------------------------------------------------------------

typedef enum {
    ASEG_GAME_HEADER = 101,
    ASEG_MAP_HEADER,
    ASEG_WORLD,
    ASEG_POLYOBJS,
    ASEG_MOBJS,
    ASEG_THINKERS,
    ASEG_SCRIPTS,
    ASEG_PLAYERS,
    ASEG_SOUNDS,
    ASEG_MISC,
    ASEG_END,
    ASEG_TEX_ARCHIVE
} gameArchiveSegment_t;

typedef enum {
    TC_NULL,
    TC_MOVE_CEILING,
    TC_VERTICAL_DOOR,
    TC_MOVE_FLOOR,
    TC_PLAT_RAISE,
    TC_INTERPRET_ACS,
    TC_FLOOR_WAGGLE,
    TC_LIGHT,
    TC_PHASE,
    TC_BUILD_PILLAR,
    TC_ROTATE_POLY,
    TC_MOVE_POLY,
    TC_POLY_DOOR
} thinkClass_t;

#pragma pack(1)
typedef struct {
    thinkClass_t tClass;
    think_t thinkerFunc;
    void    (*mangleFunc) ();
    void    (*restoreFunc) ();
    size_t  size;
} thinkInfo_t;

typedef struct {
    thinker_t thinker;
    sector_t *sector;
} ssthinker_t;

#pragma pack()

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    P_SpawnPlayer(thing_t * mthing, int pNumber);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void ArchiveWorld(void);
static void UnarchiveWorld(void);
static void ArchivePolyobjs(void);
static void UnarchivePolyobjs(void);
static void ArchiveMobjs(void);
static void UnarchiveMobjs(void);
static void ArchiveThinkers(void);
static void UnarchiveThinkers(void);
static void ArchiveScripts(void);
static void UnarchiveScripts(void);
static void ArchivePlayers(void);
static void UnarchivePlayers(void);
static void ArchiveSounds(void);
static void UnarchiveSounds(void);
static void ArchiveMisc(void);
static void UnarchiveMisc(void);
static void SetMobjArchiveNums(void);
static void RemoveAllThinkers(void);
static void MangleMobj(mobj_t *mobj);
static void RestoreMobj(mobj_t *mobj, int version);
static int GetMobjNum(mobj_t *mobj);
static void SetMobjPtr(int *archiveNum);
static void MangleSSThinker(ssthinker_t * sst);
static void RestoreSSThinker(ssthinker_t * sst);
static void RestoreSSThinkerNoSD(ssthinker_t * sst);
static void MangleScript(acs_t * script);
static void RestoreScript(acs_t * script);
static void RestorePlatRaise(plat_t * plat);
static void RestoreMoveCeiling(ceiling_t * ceiling);
static void AssertSegment(gameArchiveSegment_t segType);
static void ClearSaveSlot(int slot);
static void CopySaveSlot(int sourceSlot, int destSlot);
static void CopyFile(char *sourceName, char *destName);
static boolean ExistingFile(char *name);
static void OpenStreamOut(char *fileName);
static void CloseStreamOut(void);
void    StreamOutBuffer(void *buffer, int size);
static void StreamOutByte(byte val);
void    StreamOutWord(unsigned short val);
static void StreamOutLong(unsigned int val);
static void StreamOutFloat(float val);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int ACScriptCount;
extern byte *ActionCodeBase;
extern acsInfo_t *ACSInfo;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

char    SavePath[256] = DEFAULT_SAVEPATH;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

//static boolean kickout[MAXPLAYERS];
static int SaveToRealPlayerNum[MAXPLAYERS];
static int MobjCount;
static mobj_t **MobjList;
static int **TargetPlayerAddrs;
static int TargetPlayerCount;
static byte *SaveBuffer;
static boolean SavingPlayers;
static union {
    byte   *b;
    short  *w;
    int    *l;
    float  *f;
} SavePtr;
static LZFILE *SavingFP;

// This list has been prioritized using frequency estimates
static thinkInfo_t ThinkerInfo[] = {
    {
     TC_MOVE_FLOOR,
     T_MoveFloor,
     MangleSSThinker,
     RestoreSSThinker,
     sizeof(floormove_t)}
    ,
    {
     TC_PLAT_RAISE,
     T_PlatRaise,
     MangleSSThinker,
     RestorePlatRaise,
     sizeof(plat_t)}
    ,
    {
     TC_MOVE_CEILING,
     T_MoveCeiling,
     MangleSSThinker,
     RestoreMoveCeiling,
     sizeof(ceiling_t)}
    ,
    {
     TC_LIGHT,
     T_Light,
     MangleSSThinker,
     RestoreSSThinkerNoSD,
     sizeof(light_t)}
    ,
    {
     TC_VERTICAL_DOOR,
     T_VerticalDoor,
     MangleSSThinker,
     RestoreSSThinker,
     sizeof(vldoor_t)}
    ,
    {
     TC_PHASE,
     T_Phase,
     MangleSSThinker,
     RestoreSSThinkerNoSD,
     sizeof(phase_t)}
    ,
    {
     TC_INTERPRET_ACS,
     T_InterpretACS,
     MangleScript,
     RestoreScript,
     sizeof(acs_t)}
    ,
    {
     TC_ROTATE_POLY,
     T_RotatePoly,
     NULL,
     NULL,
     sizeof(polyevent_t)}
    ,
    {
     TC_BUILD_PILLAR,
     T_BuildPillar,
     MangleSSThinker,
     RestoreSSThinker,
     sizeof(pillar_t)}
    ,
    {
     TC_MOVE_POLY,
     T_MovePoly,
     NULL,
     NULL,
     sizeof(polyevent_t)}
    ,
    {
     TC_POLY_DOOR,
     T_PolyDoor,
     NULL,
     NULL,
     sizeof(polydoor_t)}
    ,
    {
     TC_FLOOR_WAGGLE,
     T_FloorWaggle,
     MangleSSThinker,
     RestoreSSThinker,
     sizeof(floorWaggle_t)}
    ,
    {                           // Terminator
     TC_NULL, NULL, NULL, NULL, 0}
};

// CODE --------------------------------------------------------------------

//===========================================================================
// SV_Init
//  Init the save path.
//===========================================================================
void SV_Init(void)
{
    if(ArgCheckWith("-savedir", 1))
    {
        strcpy(SavePath, ArgNext());
        // Add a trailing backslash is necessary.
        if(SavePath[strlen(SavePath) - 1] != '\\')
            strcat(SavePath, "\\");
    }
    else
    {
        // Use the default save path.
        sprintf(SavePath, DEFAULT_SAVEPATH "%s\\", G_GetVariable(DD_GAME_MODE));
    }
    M_CheckPath(SavePath);
}

//==========================================================================
//
// SV_HxSaveGame
//
//==========================================================================

void SV_HxSaveGame(int slot, char *description)
{
    char    fileName[256];
    char    versionText[HXS_VERSION_TEXT_LENGTH];

    // Open the output file
    sprintf(fileName, "%shex6.hxs", SavePath);
    M_TranslatePath(fileName, fileName);
    OpenStreamOut(fileName);

    // Write game save description
    StreamOutBuffer(description, HXS_DESCRIPTION_LENGTH);

    // Write version info
    memset(versionText, 0, HXS_VERSION_TEXT_LENGTH);
    strcpy(versionText, HXS_VERSION_TEXT);
    StreamOutBuffer(versionText, HXS_VERSION_TEXT_LENGTH);

    // Place a header marker
    StreamOutLong(ASEG_GAME_HEADER);

    // Write current map and difficulty
    StreamOutByte(gamemap);
    StreamOutByte(gameskill);
    StreamOutByte(deathmatch);
    StreamOutByte(nomonsters);
    StreamOutByte(randomclass);

    // Write global script info
    StreamOutBuffer(WorldVars, sizeof(WorldVars));
    StreamOutBuffer(ACSStore, sizeof(ACSStore));

    ArchivePlayers();

    // Place a termination marker
    StreamOutLong(ASEG_END);

    // Close the output file
    CloseStreamOut();

    // Save out the current map
    SV_HxSaveMap(true);         // true = save player info

    // Clear all save files at destination slot
    ClearSaveSlot(slot);

    // Copy base slot to destination slot
    CopySaveSlot(BASE_SLOT, slot);
}

//==========================================================================
//
// SV_HxSaveMap
//
//==========================================================================

void SV_HxSaveMap(boolean savePlayers)
{
    char    fileName[100];

    SavingPlayers = savePlayers;

    // Open the output file
    sprintf(fileName, "%shex6%02d.hxs", SavePath, gamemap);
    M_TranslatePath(fileName, fileName);
    OpenStreamOut(fileName);

    // Place a header marker
    StreamOutLong(ASEG_MAP_HEADER);

    // Write the level timer
    StreamOutLong(leveltime);

    // Set the mobj archive numbers
    SetMobjArchiveNums();
    SV_InitTextureArchives();

    ArchiveWorld();
    ArchivePolyobjs();
    ArchiveMobjs();
    ArchiveThinkers();
    ArchiveScripts();
    ArchiveSounds();
    ArchiveMisc();

    // Place a termination marker
    StreamOutLong(ASEG_END);

    // Close the output file
    CloseStreamOut();
}

//==========================================================================
//
// SV_HxLoadGame
//
//==========================================================================

void SV_HxLoadGame(int slot)
{
    int     i, k;
    char    fileName[200], buf[80];
    player_t playerBackup[MAXPLAYERS];
    mobj_t *mobj;

    // Copy all needed save files to the base slot
    if(slot != BASE_SLOT)
    {
        ClearSaveSlot(BASE_SLOT);
        CopySaveSlot(slot, BASE_SLOT);
    }

    // Create the name
    sprintf(fileName, "%shex6.hxs", SavePath);
    M_TranslatePath(fileName, fileName);

    // Load the file
    M_ReadFile(fileName, &SaveBuffer);

    // Set the save pointer and skip the description field
    SavePtr.b = SaveBuffer + HXS_DESCRIPTION_LENGTH;

    // Check the version text
    if(strcmp(SavePtr.b, HXS_VERSION_TEXT))
    {                           // Bad version
        return;
    }
    SavePtr.b += HXS_VERSION_TEXT_LENGTH;

    AssertSegment(ASEG_GAME_HEADER);

    gameepisode = 1;
    gamemap = GET_BYTE;
    gameskill = GET_BYTE;
    deathmatch = GET_BYTE;
    nomonsters = GET_BYTE;
    randomclass = GET_BYTE;

    // Read global script info
    memcpy(WorldVars, SavePtr.b, sizeof(WorldVars));
    SavePtr.b += sizeof(WorldVars);
    memcpy(ACSStore, SavePtr.b, sizeof(ACSStore));
    SavePtr.b += sizeof(ACSStore);

    // Read the player structures
    UnarchivePlayers();

    AssertSegment(ASEG_END);

    Z_Free(SaveBuffer);

    // Save player structs
    for(i = 0; i < MAXPLAYERS; i++)
    {
        playerBackup[i] = players[i];
    }

    // Load the current map
    SV_HxLoadMap();

    // Don't need the player mobj relocation info for load game
    Z_Free(TargetPlayerAddrs);

    // Restore player structs
    inv_ptr = 0;
    curpos = 0;
    for(i = 0; i < MAXPLAYERS; i++)
    {
        mobj = players[i].plr->mo;
        players[i] = playerBackup[i];
        players[i].plr->mo = mobj;
        if(i == consoleplayer)
        {
            players[i].readyArtifact = players[i].inventory[inv_ptr].type;
        }
    }

    //Con_Printf("SaveToReal: ");

    // Kick out players who do not belong here.
    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(!players[i].plr->ingame)
            continue;
        //Con_Printf("%i ", SaveToRealPlayerNum[i]);

        // Try to find a saved player that corresponds this one.
        for(k = 0; k < MAXPLAYERS; k++)
            if(SaveToRealPlayerNum[k] == i)
                break;
        if(k < MAXPLAYERS)
            continue;           // Found; don't bother this player.

        players[i].playerstate = PST_REBORN;

        if(!i)
        {
            // If the consoleplayer isn't in the save, it must be some
            // other player's file?
            P_SetMessage(players, GET_TXT(TXT_LOADMISSING));
        }
        else
        {
            NetSv_SendMessage(i, GET_TXT(TXT_LOADMISSING));

            // Kick this player out, he doesn't belong here.
            sprintf(buf, "kick %i", i);
            DD_Execute(buf, false);
        }
    }
    //Con_Printf("\n");

    /*  for(i=0; i<3; i++)
       Con_Printf("Player %i mo: %p\n", i, players[i].plr->mo); */
}

//==========================================================================
//
// SV_HxUpdateRebornSlot
//
// Copies the base slot to the reborn slot.
//
//==========================================================================

void SV_HxUpdateRebornSlot(void)
{
    ClearSaveSlot(REBORN_SLOT);
    CopySaveSlot(BASE_SLOT, REBORN_SLOT);
}

//==========================================================================
//
// SV_HxClearRebornSlot
//
//==========================================================================

void SV_HxClearRebornSlot(void)
{
    ClearSaveSlot(REBORN_SLOT);
}

//==========================================================================
//
// SV_HxMapTeleport
//
//==========================================================================

void SV_HxMapTeleport(int map, int position)
{
    int     i;
    int     j;
    char    fileName[100];
    player_t playerBackup[MAXPLAYERS];
    mobj_t *targetPlayerMobj;

    //  mobj_t *mobj;
    int     inventoryPtr;
    int     currentInvPos;
    boolean rClass;
    boolean playerWasReborn;
    boolean oldWeaponowned[NUMWEAPONS];
    int     oldKeys = 0;
    int     oldPieces = 0;
    int     bestWeapon;

    if(!deathmatch)
    {
        if(P_GetMapCluster(gamemap) == P_GetMapCluster(map))
        {                       // Same cluster - save map without saving player mobjs
            SV_HxSaveMap(false);
        }
        else
        {                       // Entering new cluster - clear base slot
            ClearSaveSlot(BASE_SLOT);
        }
    }

    // Store player structs for later
    rClass = randomclass;
    randomclass = false;
    for(i = 0; i < MAXPLAYERS; i++)
    {
        playerBackup[i] = players[i];
    }

    // Save some globals that get trashed during the load
    inventoryPtr = inv_ptr;
    currentInvPos = curpos;

    // Only SV_HxLoadMap() uses TargetPlayerAddrs, so it's NULLed here
    // for the following check (player mobj redirection)
    TargetPlayerAddrs = NULL;

    gamemap = map;
    sprintf(fileName, "%shex6%02d.hxs", SavePath, gamemap);
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
        for(i = 0; i < MAXPLAYERS; i++)
        {
            if(players[i].plr->ingame)
            {
                P_RemoveMobj(players[i].plr->mo);
            }
        }
    }

    // Restore player structs
    targetPlayerMobj = NULL;
    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(!players[i].plr->ingame)
        {
            continue;
        }
        players[i] = playerBackup[i];
        P_ClearMessage(&players[i]);
        players[i].attacker = NULL;
        players[i].poisoner = NULL;

        if(IS_NETGAME || deathmatch)
        {
            if(players[i].playerstate == PST_DEAD)
            {                   // In a network game, force all players to be alive
                players[i].playerstate = PST_REBORN;
            }
            if(!deathmatch)
            {                   // Cooperative net-play, retain keys and weapons
                oldKeys = players[i].keys;
                oldPieces = players[i].pieces;
                for(j = 0; j < NUMWEAPONS; j++)
                {
                    oldWeaponowned[j] = players[i].weaponowned[j];
                }
            }
        }
        playerWasReborn = (players[i].playerstate == PST_REBORN);
        if(deathmatch)
        {
            memset(players[i].frags, 0, sizeof(players[i].frags));
            /*mobj = P_SpawnMobj(playerstarts[0][i].x<<16,
               playerstarts[0][i].y<<16, 0, MT_PLAYER_FIGHTER);
               players[i].plr->mo = mobj; */
            players[i].plr->mo = NULL;
            G_DeathMatchSpawnPlayer(i);
            //P_RemoveMobj(mobj);
        }
        else
        {
            //P_SpawnPlayer(&playerstarts[position][i], i);
            P_SpawnPlayer(P_GetPlayerStart(position, i), i);
        }

        if(playerWasReborn && IS_NETGAME && !deathmatch)
        {                       // Restore keys and weapons when reborn in co-op
            players[i].keys = oldKeys;
            players[i].pieces = oldPieces;
            for(bestWeapon = 0, j = 0; j < NUMWEAPONS; j++)
            {
                if(oldWeaponowned[j])
                {
                    bestWeapon = j;
                    players[i].weaponowned[j] = true;
                }
            }
            players[i].ammo[MANA_1] = 25;
            players[i].ammo[MANA_2] = 25;
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

    // Redirect anything targeting a player mobj
    if(TargetPlayerAddrs)
    {
        for(i = 0; i < TargetPlayerCount; i++)
        {
            *TargetPlayerAddrs[i] = (int) targetPlayerMobj;
        }
        Z_Free(TargetPlayerAddrs);
    }

    // Destroy all things touching players
    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(players[i].plr->ingame)
        {
            P_TeleportMove(players[i].plr->mo, players[i].plr->mo->pos[VX],
                           players[i].plr->mo->pos[VY], true);
        }
    }

    // Restore trashed globals
    inv_ptr = inventoryPtr;
    curpos = currentInvPos;

    // Launch waiting scripts
    if(!deathmatch)
    {
        P_CheckACSStore();
    }

    // For single play, save immediately into the reborn slot
    if(!IS_NETGAME && !deathmatch)
    {
        SV_HxSaveGame(REBORN_SLOT, REBORN_DESCRIPTION);
    }
}

//==========================================================================
//
// SV_HxGetRebornSlot
//
//==========================================================================

int SV_HxGetRebornSlot(void)
{
    return (REBORN_SLOT);
}

//==========================================================================
//
// SV_HxRebornSlotAvailable
//
// Returns true if the reborn slot is available.
//
//==========================================================================

boolean SV_HxRebornSlotAvailable(void)
{
    char    fileName[100];

    sprintf(fileName, "%shex%d.hxs", SavePath, REBORN_SLOT);
    M_TranslatePath(fileName, fileName);
    return ExistingFile(fileName);
}

//==========================================================================
//
// SV_HxLoadMap
//
//==========================================================================

void SV_HxLoadMap(void)
{
    char    fileName[100];

#ifdef _DEBUG
    Con_Printf("SV_HxLoadMap: Begin, G_InitNew...\n");
#endif

    // We don't want to see a briefing if we're loading a map.
    brief_disabled = true;

    // Load a base level
    G_InitNew(gameskill, gameepisode, gamemap);

    // Remove all thinkers
    RemoveAllThinkers();

    // Create the name
    sprintf(fileName, "%shex6%02d.hxs", SavePath, gamemap);
    M_TranslatePath(fileName, fileName);

#ifdef _DEBUG
    Con_Printf("SV_HxLoadMap: Reading %s\n", fileName);
#endif

    // Load the file
    M_ReadFile(fileName, &SaveBuffer);
    SavePtr.b = SaveBuffer;

    AssertSegment(ASEG_MAP_HEADER);

    // Read the level timer
    leveltime = GET_LONG;

    UnarchiveWorld();
    UnarchivePolyobjs();
    UnarchiveMobjs();
    UnarchiveThinkers();
    UnarchiveScripts();
    UnarchiveSounds();
    UnarchiveMisc();

    AssertSegment(ASEG_END);

    // Free mobj list and save buffer
    Z_Free(MobjList);
    Z_Free(SaveBuffer);

    // Spawn particle generators.
    R_SetupLevel("", DDSLF_AFTER_LOADING);
}

//==========================================================================
//
// SV_HxInitBaseSlot
//
//==========================================================================

void SV_HxInitBaseSlot(void)
{
    ClearSaveSlot(BASE_SLOT);
}

//==========================================================================
// ArchivePlayer
//  Writes the given player's data (not including the ID number).
//==========================================================================
void ArchivePlayer(player_t *player)
{
    player_t temp, *p;
    ddplayer_t ddtemp, *dp;
    int     i;

    // Make a copy of the player.
    memcpy(p = &temp, player, sizeof(temp));
    memcpy(dp = &ddtemp, player->plr, sizeof(ddtemp));
    temp.plr = &ddtemp;

    // Convert the psprite states.
    for(i = 0; i < NUMPSPRITES; i++)
        if(temp.psprites[i].state)
        {
            temp.psprites[i].state =
                (state_t *) (temp.psprites[i].state - states);
        }

    // Version number. Increase when you make changes to the player data
    // segment format.
    StreamOutByte(1);

    // Class.
    StreamOutByte(cfg.PlayerClass[player - players]);

    StreamOutLong(p->playerstate);
    StreamOutLong(p->class);    // 2nd class...?
    StreamOutLong(dp->viewz);
    StreamOutLong(dp->viewheight);
    StreamOutLong(dp->deltaviewheight);
    StreamOutLong(p->bob);
    StreamOutLong(p->flyheight);
    StreamOutFloat(dp->lookdir);
    StreamOutLong(p->centering);
    StreamOutLong(p->health);
    StreamOutBuffer(p->armorpoints, sizeof(p->armorpoints));
    StreamOutBuffer(p->inventory, sizeof(p->inventory));
    StreamOutLong(p->readyArtifact);
    StreamOutLong(p->artifactCount);
    StreamOutLong(p->inventorySlotNum);
    StreamOutBuffer(p->powers, sizeof(p->powers));
    StreamOutLong(p->keys);
    StreamOutLong(p->pieces);
    StreamOutBuffer(p->frags, sizeof(p->frags));
    StreamOutLong(p->readyweapon);
    StreamOutBuffer(p->weaponowned, sizeof(p->weaponowned));
    StreamOutBuffer(p->ammo, sizeof(p->ammo));
    StreamOutLong(p->attackdown);
    StreamOutLong(p->usedown);
    StreamOutLong(p->cheats);
    StreamOutLong(p->refire);
    StreamOutLong(p->killcount);
    StreamOutLong(p->itemcount);
    StreamOutLong(p->secretcount);
    StreamOutLong(p->messageTics);
    StreamOutLong(p->ultimateMessage);
    StreamOutLong(p->yellowMessage);
    StreamOutLong(p->damagecount);
    StreamOutLong(p->bonuscount);
    StreamOutLong(p->poisoncount);
    StreamOutLong(dp->extralight);
    StreamOutLong(dp->fixedcolormap);
    StreamOutLong(p->colormap);
    StreamOutBuffer(p->psprites, sizeof(p->psprites));
    StreamOutLong(p->morphTics);
    StreamOutLong(p->jumptics);
    StreamOutLong(p->worldTimer);
}

//==========================================================================
// UnarchivePlayer
//  Reads a player's data (not including the ID number).
//==========================================================================
void UnarchivePlayer(player_t *p)
{
    ddplayer_t *dp = p->plr;
    int     i, version;

    version = GET_BYTE; // 1 for now...

    cfg.PlayerClass[p - players] = GET_BYTE;

    memset(p, 0, sizeof(*p));   // Force everything NULL,
    p->plr = dp;                // but restore the ddplayer pointer.

    p->playerstate = GET_LONG;
    p->class = GET_LONG;        // 2nd class...?
    dp->viewz = GET_LONG;
    dp->viewheight = GET_LONG;
    dp->deltaviewheight = GET_LONG;
    p->bob = GET_LONG;
    p->flyheight = GET_LONG;
    dp->lookdir = GET_FLOAT;
    p->centering = GET_LONG;
    p->health = GET_LONG;
    GET_BUFFER(p->armorpoints);
    GET_BUFFER(p->inventory);
    p->readyArtifact = GET_LONG;
    p->artifactCount = GET_LONG;
    p->inventorySlotNum = GET_LONG;
    GET_BUFFER(p->powers);
    p->keys = GET_LONG;
    p->pieces = GET_LONG;
    GET_BUFFER(p->frags);
    p->pendingweapon = p->readyweapon = GET_LONG;
    GET_BUFFER(p->weaponowned);
    GET_BUFFER(p->ammo);
    p->attackdown = GET_LONG;
    p->usedown = GET_LONG;
    p->cheats = GET_LONG;
    p->refire = GET_LONG;
    p->killcount = GET_LONG;
    p->itemcount = GET_LONG;
    p->secretcount = GET_LONG;
    p->messageTics = GET_LONG;
    p->ultimateMessage = GET_LONG;
    p->yellowMessage = GET_LONG;
    p->damagecount = GET_LONG;
    p->bonuscount = GET_LONG;
    p->poisoncount = GET_LONG;
    dp->extralight = GET_LONG;
    dp->fixedcolormap = GET_LONG;
    p->colormap = GET_LONG;
    GET_BUFFER(p->psprites);
    p->morphTics = GET_LONG;
    p->jumptics = GET_LONG;
    p->worldTimer = GET_LONG;

    // Demangle it.
    for(i = 0; i < NUMPSPRITES; i++)
        if(p->psprites[i].state)
            p->psprites[i].state = &states[(int) p->psprites[i].state];

    dp->flags |= DDPF_FIXPOS | DDPF_FIXANGLES | DDPF_FIXMOM;
    p->update |= PSF_REBORN;
}

//==========================================================================
//
// ArchivePlayers
//
//==========================================================================

static void ArchivePlayers(void)
{
    int     i;

    //int j;
    //saveplayer_t tempPlayer;

    StreamOutLong(ASEG_PLAYERS);
    for(i = 0; i < MAXPLAYERS; i++)
    {
        StreamOutByte(players[i].plr->ingame);
    }
    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(!players[i].plr->ingame)
            continue;

        StreamOutLong(Net_GetPlayerID(i));
        ArchivePlayer(players + i);

        /*StreamOutByte(cfg.PlayerClass[i]);
           //tempPlayer = players[i];
           PlayerConverter(players+i, &tempPlayer, true);

           // Convert the psprite states.
           for(j = 0; j < NUMPSPRITES; j++)
           {
           if(tempPlayer.psprites[j].state)
           {
           tempPlayer.psprites[j].state =
           (state_t *)(tempPlayer.psprites[j].state-states);
           }
           }
           StreamOutBuffer(&tempPlayer, sizeof(tempPlayer)); */
    }
}

//==========================================================================
//
// UnarchivePlayers
//
//==========================================================================

static void UnarchivePlayers(void)
{
    /*    int       i;
       int      j;
       unsigned int pid;
       player_t dummy_player;
       ddplayer_t dummy_ddplayer;
       player_t *player;

       // Setup the dummy.
       dummy_player.plr = &dummy_ddplayer;

       for(i=0; i<MAXPLAYERS; i++)
       {
       if(!infile[i]) continue;

       // The ID number will determine which player this actually is.
       pid = SV_ReadLong();
       for(player=0, j=0; j<MAXPLAYERS; j++)
       if(gi.GetPlayerID(j) == pid)
       {
       // This is our guy.
       player = players + j;
       loaded[j] = true;
       break;
       }
       if(!player)
       {
       // We have a missing player. Use a dummy to load the data.
       player = &dummy_player;
       }
       SV_ReadPlayer(player);
     */

    //int i, j;
    //saveplayer_t tempPlayer;

    int     i;
    int     j;
    unsigned int pid;
    player_t dummy_player;
    ddplayer_t dummy_ddplayer;
    player_t *player;
    boolean infile[MAXPLAYERS], loaded[MAXPLAYERS];

    AssertSegment(ASEG_PLAYERS);

    // Savegames do not have the power to say who's in the game and
    // who isn't. The clients currently connected are "ingame", not
    // anyone else.

    memset(loaded, 0, sizeof(loaded));
    dummy_player.plr = &dummy_ddplayer; // Setup the dummy.

    // See how many players was saved.
    for(i = 0; i < MAXPLAYERS; i++)
        infile[i] = GET_BYTE;

    // Load the data of those players.
    for(i = 0; i < MAXPLAYERS; i++)
    {
        // By default a saved player translates to nothing.
        SaveToRealPlayerNum[i] = -1;

        if(!infile[i])
            continue;

        // The ID number will determine which player this actually is.
        pid = GET_LONG;
        for(player = 0, j = 0; j < MAXPLAYERS; j++)
            if((IS_NETGAME && Net_GetPlayerID(j) == pid) ||
               (!IS_NETGAME && j == 0))
            {
                // This is our guy.
                player = players + j;
                loaded[j] = true;
                // Later references to the player number 'i' must be
                // translated!
                SaveToRealPlayerNum[i] = j;
                break;
            }
        if(!player)
        {
            // We have a missing player. Use a dummy to load the data.
            player = &dummy_player;
        }

        // Read the data.
        UnarchivePlayer(player);

        /*cfg.PlayerClass[i] = GET_BYTE;

           memcpy(&tempPlayer, SavePtr.b, sizeof(tempPlayer));
           SavePtr.b += sizeof(tempPlayer);
           PlayerConverter(players+i, &tempPlayer, false); */

        /*      players[i].plr->mo = NULL; // Will be set when unarc thinker
           P_ClearMessage(&players[i]);
           players[i].attacker = NULL;
           players[i].poisoner = NULL;
           for(j = 0; j < NUMPSPRITES; j++)
           {
           if(players[i].psprites[j].state)
           {
           players[i].psprites[j].state =
           &states[(int)players[i].psprites[j].state];
           }
           } */
    }

    /*  // Notify the players that weren't in the savegame.
       // (And kick them out!)
       for(i=0; i<MAXPLAYERS; i++)
       if(!loaded[i] && players[i].plr->ingame)
       kickout[i] = true; */
}

//==========================================================================
//
// ArchiveWorld
//
//==========================================================================

void ArchiveWorld(void)
{
    int     i;
    int     j;
    sector_t *sec;
    xsector_t* xsec;
    line_t *li;
    xline_t *xli;
    side_t *si;
    byte    rgb[4];

    // First the texture archive.
    StreamOutLong(ASEG_TEX_ARCHIVE);
    SV_WriteTextureArchive();

    StreamOutLong(ASEG_WORLD);
    for(i = 0; i < DD_GetInteger(DD_SECTOR_COUNT); i++)
    {
        sec = P_ToPtr(DMU_SECTOR, i);
        xsec = &xsectors[i];
        StreamOutWord(P_GetIntp(sec, DMU_FLOOR_HEIGHT));
        StreamOutWord(P_GetIntp(sec, DMU_CEILING_HEIGHT));
        StreamOutWord(SV_FlatArchiveNum(P_GetIntp(sec, DMU_FLOOR_TEXTURE)));
        StreamOutWord(SV_FlatArchiveNum(P_GetIntp(sec, DMU_CEILING_TEXTURE)));
        StreamOutWord(P_GetIntp(sec, DMU_LIGHT_LEVEL));
        P_GetBytepv(sec, DMU_COLOR, rgb); StreamOutBuffer(rgb, 3);
        P_GetBytepv(sec, DMU_FLOOR_COLOR, rgb); StreamOutBuffer(rgb, 3);
        P_GetBytepv(sec, DMU_CEILING_COLOR, rgb); StreamOutBuffer(rgb, 3);
        StreamOutWord(xsec->special);
        StreamOutWord(xsec->tag);
        StreamOutWord(xsec->seqType);
        StreamOutFloat(P_GetFloatp(sec, DMU_FLOOR_OFFSET_X));
        StreamOutFloat(P_GetFloatp(sec, DMU_FLOOR_OFFSET_Y));
        StreamOutFloat(P_GetFloatp(sec, DMU_CEILING_OFFSET_X));
        StreamOutFloat(P_GetFloatp(sec, DMU_CEILING_OFFSET_Y));
    }
    for(i = 0; i < DD_GetInteger(DD_LINE_COUNT); i++)
    {
        li = P_ToPtr(DMU_LINE, i);
        xli = &xlines[i];
        StreamOutWord(P_GetIntp(li, DMU_FLAGS));
        StreamOutByte(xli->special);
        StreamOutByte(xli->arg1);
        StreamOutByte(xli->arg2);
        StreamOutByte(xli->arg3);
        StreamOutByte(xli->arg4);
        StreamOutByte(xli->arg5);
        for(j = 0; j < 2; j++)
        {
            // TODO: Make sure that NO_INDEX is always interpreted as
            // -1 (because this is a short int originally).
            int sd = P_GetIntp(li, DMU_SIDE0 + j);
            if(sd == NO_INDEX)
            {
                continue;
            }
            si = P_ToPtr(DMU_SIDE, sd);
            StreamOutWord(P_GetIntp(si, DMU_TEXTURE_OFFSET_X));
            StreamOutWord(P_GetIntp(si, DMU_TEXTURE_OFFSET_Y));
            StreamOutWord(SV_TextureArchiveNum(P_GetIntp(si, DMU_TOP_TEXTURE)));
            StreamOutWord(SV_TextureArchiveNum(P_GetIntp(si, DMU_BOTTOM_TEXTURE)));
            StreamOutWord(SV_TextureArchiveNum(P_GetIntp(si, DMU_MIDDLE_TEXTURE)));

            P_GetBytepv(si, DMU_TOP_COLOR, rgb); StreamOutBuffer(rgb, 3);
            P_GetBytepv(si, DMU_MIDDLE_COLOR, rgb); StreamOutBuffer(rgb, 4);
            P_GetBytepv(si, DMU_BOTTOM_COLOR, rgb); StreamOutBuffer(rgb, 3);
            StreamOutWord(P_GetIntp(si, DMU_MIDDLE_BLENDMODE));
            StreamOutWord(P_GetIntp(si, DMU_FLAGS));
        }
    }
}

//==========================================================================
//
// UnarchiveWorld
//
//==========================================================================

void UnarchiveWorld(void)
{
    int     i;
    int     j;
    sector_t *sec;
    xsector_t *xsec;
    line_t *li;
    xline_t *xli;
    side_t *si;
    byte rgb[4];

    AssertSegment(ASEG_TEX_ARCHIVE);
    SV_ReadTextureArchive();

    AssertSegment(ASEG_WORLD);
    for(i = 0; i < DD_GetInteger(DD_SECTOR_COUNT); i++)
    {
        int fh = GET_WORD;
        int ch = GET_WORD;

        sec = P_ToPtr(DMU_SECTOR, i);
        xsec = &xsectors[i];

        P_SetIntp(sec, DMU_FLOOR_HEIGHT, fh);
        P_SetIntp(sec, DMU_CEILING_HEIGHT, ch);

        // Update the "target heights" of the planes.
        P_SetIntp(sec, DMU_FLOOR_TARGET, fh);
        P_SetIntp(sec, DMU_CEILING_TARGET, ch);

        // The move speed is not saved; can cause minor problems.
        P_SetIntp(sec, DMU_FLOOR_SPEED, 0);
        P_SetIntp(sec, DMU_CEILING_SPEED, 0);

        P_SetIntp(sec, DMU_FLOOR_TEXTURE, SV_GetArchiveFlat(GET_WORD));
        P_SetIntp(sec, DMU_CEILING_TEXTURE, SV_GetArchiveFlat(GET_WORD));
        P_SetIntp(sec, DMU_LIGHT_LEVEL, GET_WORD);
        GET_DATA(rgb, 3); P_SetBytepv(sec, DMU_COLOR, rgb);
        GET_DATA(rgb, 3); P_SetBytepv(sec, DMU_FLOOR_COLOR, rgb);
        GET_DATA(rgb, 3); P_SetBytepv(sec, DMU_CEILING_COLOR, rgb);
        xsec->special = GET_WORD;
        xsec->tag = GET_WORD;
        xsec->seqType = GET_WORD;
        P_SetFloatp(sec, DMU_FLOOR_OFFSET_X, GET_FLOAT);
        P_SetFloatp(sec, DMU_FLOOR_OFFSET_Y, GET_FLOAT);
        P_SetFloatp(sec, DMU_CEILING_OFFSET_X, GET_FLOAT);
        P_SetFloatp(sec, DMU_CEILING_OFFSET_Y, GET_FLOAT);
        xsec->specialdata = 0;
        xsec->soundtarget = 0;
    }
    for(i = 0; i < DD_GetInteger(DD_LINE_COUNT); i++)
    {
        li = P_ToPtr(DMU_LINE, i);
        xli = &xlines[i];

        P_SetIntp(li, DMU_FLAGS, GET_WORD);
        xli->special = GET_BYTE;
        xli->arg1 = GET_BYTE;
        xli->arg2 = GET_BYTE;
        xli->arg3 = GET_BYTE;
        xli->arg4 = GET_BYTE;
        xli->arg5 = GET_BYTE;
        for(j = 0; j < 2; j++)
        {
            int sdnum = P_GetIntp(li, DMU_SIDE0 + j);
            if(sdnum == NO_INDEX)
            {
                continue;
            }
            si = P_ToPtr(DMU_SIDE, sdnum);
            P_SetIntp(si, DMU_TEXTURE_OFFSET_X, GET_WORD);
            P_SetIntp(si, DMU_TEXTURE_OFFSET_Y, GET_WORD);
            P_SetIntp(si, DMU_TOP_TEXTURE, SV_GetArchiveTexture(GET_WORD));
            P_SetIntp(si, DMU_BOTTOM_TEXTURE, SV_GetArchiveTexture(GET_WORD));
            P_SetIntp(si, DMU_MIDDLE_TEXTURE, SV_GetArchiveTexture(GET_WORD));

            GET_DATA(rgb, 3); P_SetBytepv(si, DMU_TOP_COLOR, rgb);
            GET_DATA(rgb, 4); P_SetBytepv(si, DMU_MIDDLE_COLOR, rgb);
            GET_DATA(rgb, 3); P_SetBytepv(si, DMU_BOTTOM_COLOR, rgb);
            P_SetIntp(si, DMU_MIDDLE_BLENDMODE, GET_WORD);
            P_SetIntp(si, DMU_FLAGS, GET_WORD);
        }
    }
}

//==========================================================================
//
// SetMobjArchiveNums
//
// Sets the archive numbers in all mobj structs.  Also sets the MobjCount
// global.  Ignores player mobjs if SavingPlayers is false.
//
//==========================================================================

static void SetMobjArchiveNums(void)
{
    mobj_t *mobj;
    thinker_t *thinker;
    int     i;

    MobjCount = 0;

    // jk: I don't know if it is ever happens, but what if a mobj
    // has a target that isn't archived? (doesn't have a thinker).
    // Let's initialize the archiveNums of all known mobjs to -1.
    for(i = 0; i < DD_GetInteger(DD_SECTOR_COUNT); i++)
    {
        for(mobj = P_GetPtr(DMU_SECTOR, i, DMU_THINGS); mobj; mobj = mobj->snext)
            mobj->archiveNum = MOBJ_NULL;
    }

    for(thinker = thinkercap.next; thinker != &thinkercap && thinker;
        thinker = thinker->next)
    {
        if(thinker->function == P_MobjThinker)
        {
            mobj = (mobj_t *) thinker;
            if(mobj->player && !SavingPlayers)
            {                   // Skipping player mobjs
                continue;
            }
            mobj->archiveNum = MobjCount++;
        }
    }
}

//==========================================================================
// ArchiveMobj
//==========================================================================
void ArchiveMobj(mobj_t *original)
{
    mobj_t  temp, *mo;

    memcpy(mo = &temp, original, sizeof(*mo));
    MangleMobj(mo);

    // Version number.
    // 2: Added the 'translucency' byte.
    // 3: Added byte 'vistarget'
    // 4: Added long 'tracer'
    StreamOutByte(4);

    StreamOutLong(mo->pos[VX]);
    StreamOutLong(mo->pos[VY]);
    StreamOutLong(mo->pos[VZ]);
    StreamOutLong(mo->angle);
    StreamOutLong(mo->sprite);
    StreamOutLong(mo->frame);
    StreamOutLong(mo->floorpic);
    StreamOutLong(mo->radius);
    StreamOutLong(mo->height);
    StreamOutLong(mo->momx);
    StreamOutLong(mo->momy);
    StreamOutLong(mo->momz);
    StreamOutLong(mo->valid);
    StreamOutLong(mo->type);
    StreamOutLong((int) mo->info);
    StreamOutLong(mo->tics);
    StreamOutLong((int) mo->state);
    StreamOutLong(mo->damage);
    StreamOutLong(mo->flags);
    StreamOutLong(mo->flags2);
    StreamOutLong(mo->special1);
    StreamOutLong(mo->special2);
    StreamOutLong(mo->health);
    StreamOutLong(mo->movedir);
    StreamOutLong(mo->movecount);
    StreamOutLong((int) mo->target);
    StreamOutLong(mo->reactiontime);
    StreamOutLong(mo->threshold);
    StreamOutLong((int) mo->player);
    StreamOutLong(mo->lastlook);
    StreamOutLong(mo->floorclip);
    StreamOutLong(mo->archiveNum);
    StreamOutLong(mo->tid);
    StreamOutLong(mo->special);
    StreamOutBuffer(mo->args, sizeof(mo->args));
    StreamOutByte(mo->translucency);
    StreamOutByte((byte)(mo->vistarget +1));
    StreamOutLong((int) mo->tracer);
}

//==========================================================================
// UnarchiveMobj
//==========================================================================
void UnarchiveMobj(mobj_t *mo)
{
    int     version = GET_BYTE;

    memset(mo, 0, sizeof(*mo));
    mo->pos[VX] = GET_LONG;
    mo->pos[VY] = GET_LONG;
    mo->pos[VZ] = GET_LONG;
    mo->angle = GET_LONG;
    mo->sprite = GET_LONG;
    mo->frame = GET_LONG;
    mo->floorpic = GET_LONG;
    mo->radius = GET_LONG;
    mo->height = GET_LONG;
    mo->momx = GET_LONG;
    mo->momy = GET_LONG;
    mo->momz = GET_LONG;
    mo->valid = GET_LONG;
    mo->type = GET_LONG;
    mo->info = (mobjinfo_t *) GET_LONG;
    mo->tics = GET_LONG;
    mo->state = (state_t *) GET_LONG;
    mo->damage = GET_LONG;
    mo->flags = GET_LONG;
    mo->flags2 = GET_LONG;
    mo->special1 = GET_LONG;
    mo->special2 = GET_LONG;
    mo->health = GET_LONG;
    mo->movedir = GET_LONG;
    mo->movecount = GET_LONG;
    mo->target = (mobj_t *) GET_LONG;
    mo->reactiontime = GET_LONG;
    mo->threshold = GET_LONG;
    mo->player = (player_t *) GET_LONG;
    mo->lastlook = GET_LONG;
    mo->floorclip = GET_LONG;
    mo->archiveNum = GET_LONG;
    mo->tid = GET_LONG;
    mo->special = GET_LONG;
    GET_BUFFER(mo->args);

    if(version >= 2)
    {
        // Version 2 added the 'translucency' byte.
        mo->translucency = GET_BYTE;
    }

    if(version >= 3)
    {
        // Ver3 has the vistarget byte.
        mo->vistarget = (GET_BYTE) -1;
    }

    if(version >= 4)
        mo->tracer = (mobj_t *) GET_LONG;

    RestoreMobj(mo, version);
}

//==========================================================================
//
// ArchiveMobjs
//
//==========================================================================

static void ArchiveMobjs(void)
{
    int     count;
    thinker_t *thinker;

    //  savemobj_t tempMobj;

    StreamOutLong(ASEG_MOBJS);
    StreamOutLong(MobjCount);
    count = 0;
    for(thinker = thinkercap.next; thinker != &thinkercap && thinker;
        thinker = thinker->next)
    {
        if(thinker->function != P_MobjThinker)
        {                       // Not a mobj thinker
            continue;
        }
        if(((mobj_t *) thinker)->player && !SavingPlayers)
        {                       // Skipping player mobjs
            continue;
        }
        count++;
        //  memcpy(&tempMobj, thinker, sizeof(mobj_t));
        /*MobjConverter( (mobj_t*) thinker, &tempMobj, true);
           MangleMobj(&tempMobj);
           StreamOutBuffer(&tempMobj, sizeof(tempMobj)); */
        ArchiveMobj((mobj_t *) thinker);
    }
    if(count != MobjCount)
    {
        Con_Error("ArchiveMobjs: bad mobj count");
    }
}

//==========================================================================
//
// UnarchiveMobjs
//
//==========================================================================

static void UnarchiveMobjs(void)
{
    int     i;
    mobj_t *mobj;

    //  savemobj_t tempMobj;

    DBG(printf("UnarchiveMobjs\n"));

    AssertSegment(ASEG_MOBJS);

    DBG(printf("- assertion succeeded\n"));

    TargetPlayerAddrs =
        Z_Malloc(MAX_TARGET_PLAYERS * sizeof(int *), PU_STATIC, NULL);
    TargetPlayerCount = 0;
    MobjCount = GET_LONG;

    DBG(printf("- MobjCount: %d\n", MobjCount));

    MobjList = Z_Malloc(MobjCount * sizeof(mobj_t *), PU_STATIC, NULL);
    for(i = 0; i < MobjCount; i++)
    {
        MobjList[i] = Z_Malloc(sizeof(mobj_t), PU_LEVEL, NULL);
    }
    DBG(printf
        ("- memory allocated for each mobj (sizeof = %d bytes)\n",
         sizeof(mobj_t)));
    for(i = 0; i < MobjCount; i++)
    {
        DBG(printf("- loading mobj %d\n", i));

        mobj = MobjList[i];

        UnarchiveMobj(mobj);

        /*memcpy(&tempMobj, SavePtr.b, sizeof(tempMobj));
           SavePtr.b += sizeof(tempMobj);
           MobjConverter(mobj, &tempMobj, false);
           mobj->thinker.function = P_MobjThinker; */
        //DBG(printf( "   + target: %d\n", mobj->target));
        //RestoreMobj(mobj);

        //if(mobj->dplayer && !mobj->dplayer->ingame)
        if(mobj->player == INVALID_PLAYER)
        {
            // This mobj doesn't belong to anyone any more.
            //mobj->dplayer->mo = NULL;
            Z_Free(mobj);
            MobjList[i] = NULL; // The mobj no longer exists.
            continue;
        }

        mobj->thinker.function = P_MobjThinker;
        P_AddThinker(&mobj->thinker);
    }
    P_CreateTIDList();
    P_InitCreatureCorpseQueue(true);    // true = scan for corpses
}

//==========================================================================
//
// MangleMobj
//
//==========================================================================

static void MangleMobj(mobj_t *mobj)
{
    boolean corpse;

    corpse = mobj->flags & MF_CORPSE;
    mobj->state = (state_t *) (mobj->state - states);
    if(mobj->player)
    {
        mobj->player = (player_t *) ((mobj->player - players) + 1);
    }
    if(corpse)
    {
        mobj->target = (mobj_t *) MOBJ_NULL;
    }
    else
    {
        mobj->target = (mobj_t *) GetMobjNum(mobj->target);
    }
    switch (mobj->type)
    {
        // Just tracer
    case MT_BISH_FX:
    case MT_HOLY_FX:
    case MT_DRAGON:
    case MT_THRUSTFLOOR_UP:
    case MT_THRUSTFLOOR_DOWN:
    case MT_MINOTAUR:
    case MT_SORCFX1:
    case MT_MSTAFF_FX2:
        if(corpse)
        {
            mobj->tracer = (mobj_t *) MOBJ_NULL;
        }
        else
        {
            mobj->tracer = (mobj_t *) GetMobjNum(mobj->tracer);
        }
        break;

        // Just special2
    case MT_LIGHTNING_FLOOR:
    case MT_LIGHTNING_ZAP:
        if(corpse)
        {
            mobj->special2 = MOBJ_NULL;
        }
        else
        {
            mobj->special2 = GetMobjNum((mobj_t *) mobj->special2);
        }
        break;

        // Both tracer and special2
    case MT_HOLY_TAIL:
    case MT_LIGHTNING_CEILING:
        if(corpse)
        {
            mobj->tracer = (mobj_t *) MOBJ_NULL;
            mobj->special2 = MOBJ_NULL;
        }
        else
        {
            mobj->tracer = (mobj_t *) GetMobjNum(mobj->tracer);
            mobj->special2 = GetMobjNum((mobj_t *) mobj->special2);
        }
        break;

        // Miscellaneous
    case MT_KORAX:
        mobj->special1 = 0;     // Searching index
        break;

    default:
        break;
    }
}

//==========================================================================
//
// GetMobjNum
//
//==========================================================================

static int GetMobjNum(mobj_t *mobj)
{
    if(mobj == NULL)
    {
        return MOBJ_NULL;
    }
    if(mobj->player && !SavingPlayers)
    {
        return MOBJ_XX_PLAYER;
    }
    return mobj->archiveNum;
}

//==========================================================================
//
// RestoreMobj
//
//==========================================================================

static void RestoreMobj(mobj_t *mobj, int ver)
{
    // Restore DDMF flags set only in P_SpawnMobj. R_SetAllDoomsdayFlags
    // might not set these because it only iterates seclinked mobjs.
    if(mobj->flags & MF_SOLID)
    {
        mobj->ddflags |= DDMF_SOLID;
    }
    if(mobj->flags2 & MF2_DONTDRAW)
    {
        mobj->ddflags |= DDMF_DONTDRAW;
    }

    mobj->visangle = mobj->angle >> 16;
    mobj->state = &states[(int) mobj->state];
    if(mobj->player)
    {
        // The player number translation table is used to find out the
        // *current* (actual) player number of the referenced player.
        int     pNum = SaveToRealPlayerNum[(int) mobj->player - 1];

        if(pNum < 0)
        {
            // This saved player does not exist in the current game!
            // This'll make the mobj unarchiver destroy this mobj.
            mobj->player = INVALID_PLAYER;
            return;
        }
        mobj->player = &players[pNum];
        mobj->dplayer = mobj->player->plr;
        mobj->dplayer->mo = mobj;
    }
    P_SetThingPosition(mobj);
    mobj->info = &mobjinfo[mobj->type];
    mobj->floorz = P_GetFixedp(mobj->subsector, DMU_SECTOR_OF_SUBSECTOR | DMU_FLOOR_HEIGHT);
    mobj->ceilingz = P_GetFixedp(mobj->subsector, DMU_SECTOR_OF_SUBSECTOR | DMU_CEILING_HEIGHT);
    SetMobjPtr((int *) &mobj->target);
    switch (mobj->type)
    {
        // Just tracer
    case MT_BISH_FX:
    case MT_HOLY_FX:
    case MT_DRAGON:
    case MT_THRUSTFLOOR_UP:
    case MT_THRUSTFLOOR_DOWN:
    case MT_MINOTAUR:
    case MT_SORCFX1:
        if(ver >= 4)
            SetMobjPtr((int *) &mobj->tracer);
        else
            SetMobjPtr((int *) &mobj->special1);
        break;

        // Just special2
    case MT_LIGHTNING_FLOOR:
    case MT_LIGHTNING_ZAP:
        SetMobjPtr(&mobj->special2);
        break;

        // Both tracer and special2
    case MT_HOLY_TAIL:
    case MT_LIGHTNING_CEILING:
        if(ver >= 4)
            SetMobjPtr((int *) &mobj->tracer);
        else
            SetMobjPtr((int *) &mobj->special1);
        SetMobjPtr(&mobj->special2);
        break;

    default:
        break;
    }
}

//==========================================================================
//
// SetMobjPtr
//
//==========================================================================

static void SetMobjPtr(int *archiveNum)
{
    if(*archiveNum == MOBJ_NULL)
    {
        *archiveNum = 0;
        return;
    }
    if(*archiveNum == MOBJ_XX_PLAYER)
    {
        if(TargetPlayerCount == MAX_TARGET_PLAYERS)
        {
            Con_Error("RestoreMobj: exceeded MAX_TARGET_PLAYERS");
        }
        TargetPlayerAddrs[TargetPlayerCount++] = archiveNum;
        *archiveNum = 0;
        return;
    }
    // Check that the archiveNum is valid. -jk
    if(*archiveNum < 0 || *archiveNum > MobjCount - 1)
    {
        *archiveNum = 0;        // Set it to null. What else can we do?
        return;
    }
    *archiveNum = (int) MobjList[*archiveNum];
}

//==========================================================================
//
// ArchiveThinkers
//
//==========================================================================

static void ArchiveThinkers(void)
{
    thinker_t *thinker;
    thinkInfo_t *info;
    byte    buffer[MAX_THINKER_SIZE];

    StreamOutLong(ASEG_THINKERS);
    for(thinker = thinkercap.next; thinker != &thinkercap && thinker;
        thinker = thinker->next)
    {
        for(info = ThinkerInfo; info->tClass != TC_NULL; info++)
        {
            if(thinker->function == info->thinkerFunc)
            {
                StreamOutByte(info->tClass);
                memcpy(buffer, thinker, info->size);
                if(info->mangleFunc)
                {
                    info->mangleFunc(buffer);
                }
                StreamOutBuffer(buffer, info->size);
                break;
            }
        }
    }
    // Add a termination marker
    StreamOutByte(TC_NULL);
}

//==========================================================================
//
// UnarchiveThinkers
//
//==========================================================================

static void UnarchiveThinkers(void)
{
    int     tClass;
    thinker_t *thinker;
    thinkInfo_t *info;

    AssertSegment(ASEG_THINKERS);
    while((tClass = GET_BYTE) != TC_NULL)
    {
        for(info = ThinkerInfo; info->tClass != TC_NULL; info++)
        {
            if(tClass == info->tClass)
            {
                thinker = Z_Malloc(info->size, PU_LEVEL, NULL);
                memcpy(thinker, SavePtr.b, info->size);
                SavePtr.b += info->size;
                thinker->function = info->thinkerFunc;
                if(info->restoreFunc)
                {
                    info->restoreFunc(thinker);
                }
                P_AddThinker(thinker);
                break;
            }
        }
        if(info->tClass == TC_NULL)
        {
            Con_Error("UnarchiveThinkers: Unknown tClass %d in " "savegame",
                      tClass);
        }
    }
}

//==========================================================================
//
// MangleSSThinker
//
//==========================================================================

static void MangleSSThinker(ssthinker_t * sst)
{
    sst->sector = (sector_t*) P_ToIndex(sst->sector);
}

//==========================================================================
//
// RestoreSSThinker
//
//==========================================================================

static void RestoreSSThinker(ssthinker_t * sst)
{
    sst->sector = P_ToPtr(DMU_SECTOR, (int) sst->sector);
    P_XSector(sst->sector)->specialdata = sst->thinker.function;
}

//==========================================================================
//
// RestoreSSThinkerNoSD
//
//==========================================================================

static void RestoreSSThinkerNoSD(ssthinker_t * sst)
{
    sst->sector = P_ToPtr(DMU_SECTOR, (int) sst->sector);
}

//==========================================================================
//
// MangleScript
//
//==========================================================================

static void MangleScript(acs_t * script)
{
    script->ip = (int *) ((int) (script->ip) - (int) ActionCodeBase);
    script->line =
        script->line ? (line_t *) P_ToIndex(script->line) : (line_t *) -1;
    script->activator = (mobj_t *) GetMobjNum(script->activator);
}

//==========================================================================
//
// RestoreScript
//
//==========================================================================

static void RestoreScript(acs_t * script)
{
    script->ip = (int *) (ActionCodeBase + (int) script->ip);
    if((int) script->line == -1)
    {
        script->line = NULL;
    }
    else
    {
        script->line = P_ToPtr(DMU_LINE, (int) script->line);
    }
    SetMobjPtr((int *) &script->activator);
}

//==========================================================================
//
// RestorePlatRaise
//
//==========================================================================

static void RestorePlatRaise(plat_t * plat)
{
    plat->sector = P_ToPtr(DMU_SECTOR, (int) plat->sector);
    P_XSector(plat->sector)->specialdata = T_PlatRaise;
    P_AddActivePlat(plat);
}

//==========================================================================
//
// RestoreMoveCeiling
//
//==========================================================================

static void RestoreMoveCeiling(ceiling_t * ceiling)
{
    ceiling->sector = P_ToPtr(DMU_SECTOR, (int) ceiling->sector);
    P_XSector(ceiling->sector)->specialdata = T_MoveCeiling;
    P_AddActiveCeiling(ceiling);
}

//==========================================================================
//
// ArchiveScripts
//
//==========================================================================

static void ArchiveScripts(void)
{
    int     i;

    StreamOutLong(ASEG_SCRIPTS);
    for(i = 0; i < ACScriptCount; i++)
    {
        StreamOutWord(ACSInfo[i].state);
        StreamOutWord(ACSInfo[i].waitValue);
    }
    StreamOutBuffer(MapVars, sizeof(MapVars));
}

//==========================================================================
//
// UnarchiveScripts
//
//==========================================================================

static void UnarchiveScripts(void)
{
    int     i;

    AssertSegment(ASEG_SCRIPTS);
    for(i = 0; i < ACScriptCount; i++)
    {
        ACSInfo[i].state = GET_WORD;
        ACSInfo[i].waitValue = GET_WORD;
    }
    memcpy(MapVars, SavePtr.b, sizeof(MapVars));
    SavePtr.b += sizeof(MapVars);
}

//==========================================================================
//
// ArchiveMisc
//
//==========================================================================

static void ArchiveMisc(void)
{
    int     ix;

    StreamOutLong(ASEG_MISC);
    for(ix = 0; ix < MAXPLAYERS; ix++)
    {
        StreamOutLong(localQuakeHappening[ix]);
    }
}

//==========================================================================
//
// UnarchiveMisc
//
//==========================================================================

static void UnarchiveMisc(void)
{
    int     ix;

    AssertSegment(ASEG_MISC);
    for(ix = 0; ix < MAXPLAYERS; ix++)
    {
        localQuakeHappening[ix] = GET_LONG;
    }
}

//==========================================================================
//
// RemoveAllThinkers
//
//==========================================================================

static void RemoveAllThinkers(void)
{
    thinker_t *thinker;
    thinker_t *nextThinker;

    thinker = thinkercap.next;
    while(thinker != &thinkercap && thinker)
    {
        nextThinker = thinker->next;
        if(thinker->function == P_MobjThinker)
        {
            P_RemoveMobj((mobj_t *) thinker);
        }
        else
        {
            Z_Free(thinker);
        }
        thinker = nextThinker;
    }
    P_InitThinkers();
}

//==========================================================================
//
// ArchiveSounds
//
//==========================================================================

static void ArchiveSounds(void)
{
    seqnode_t *node;
    sector_t *sec;
    int     difference;
    int     i;

    StreamOutLong(ASEG_SOUNDS);

    // Save the sound sequences
    StreamOutLong(ActiveSequences);
    for(node = SequenceListHead; node; node = node->next)
    {
        StreamOutLong(node->sequence);
        StreamOutLong(node->delayTics);
        StreamOutLong(node->volume);
        StreamOutLong(SN_GetSequenceOffset(node->sequence, node->sequencePtr));
        StreamOutLong(node->currentSoundID);
        for(i = 0; i < numpolyobjs; i++)
        {
            if(node->mobj == P_GetPtr(DMU_POLYOBJ, i, DMU_START_SPOT))
            {
                break;
            }
        }
        if(i == DD_GetInteger(DD_POLYOBJ_COUNT))
        {                       // Sound is attached to a sector, not a polyobj
            sec = P_GetPtrp(R_PointInSubsector(node->mobj->pos[VX], node->mobj->pos[VY]),
                            DMU_SECTOR);
            difference = P_ToIndex(sec);
            StreamOutLong(0);   // 0 -- sector sound origin
        }
        else
        {
            StreamOutLong(1);   // 1 -- polyobj sound origin
            difference = i;
        }
        StreamOutLong(difference);
    }
}

//==========================================================================
//
// UnarchiveSounds
//
//==========================================================================

static void UnarchiveSounds(void)
{
    int     i;
    int     numSequences;
    int     sequence;
    int     delayTics;
    int     volume;
    int     seqOffset;
    int     soundID;
    int     polySnd;
    int     secNum;
    mobj_t *sndMobj;

    AssertSegment(ASEG_SOUNDS);

    // Reload and restart all sound sequences
    numSequences = GET_LONG;
    i = 0;
    while(i < numSequences)
    {
        sequence = GET_LONG;
        delayTics = GET_LONG;
        volume = GET_LONG;
        seqOffset = GET_LONG;

        soundID = GET_LONG;
        polySnd = GET_LONG;
        secNum = GET_LONG;
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

//==========================================================================
//
// ArchivePolyobjs
//
//==========================================================================

static void ArchivePolyobjs(void)
{
    int     i;
    int     count = DD_GetInteger(DD_POLYOBJ_COUNT);

    StreamOutLong(ASEG_POLYOBJS);
    StreamOutLong(count);
    for(i = 0; i < count; i++)
    {
        polyobj_t* po = P_ToPtr(DMU_POLYOBJ, i);
        StreamOutLong(P_GetIntp(po, DMU_TAG));
        StreamOutLong(P_GetAnglep(po, DMU_ANGLE));
        StreamOutLong(P_GetFixedp(po, DMU_START_SPOT_X));
        StreamOutLong(P_GetFixedp(po, DMU_START_SPOT_Y));
    }
}

//==========================================================================
//
// UnarchivePolyobjs
//
//==========================================================================

static void UnarchivePolyobjs(void)
{
    int     i;
    fixed_t deltaX;
    fixed_t deltaY;
    angle_t angle;
    int     count = DD_GetInteger(DD_POLYOBJ_COUNT);

    AssertSegment(ASEG_POLYOBJS);
    if(GET_LONG != count)
    {
        Con_Error("UnarchivePolyobjs: Bad polyobj count");
    }
    for(i = 0; i < count; i++)
    {
        polyobj_t* po = P_ToPtr(DMU_POLYOBJ, i);
        if(GET_LONG != P_GetIntp(po, DMU_TAG))
        {
            Con_Error("UnarchivePolyobjs: Invalid polyobj tag");
        }
        angle = (angle_t) GET_LONG;
        PO_RotatePolyobj(P_GetIntp(po, DMU_TAG), angle);
        P_SetAnglep(po, DMU_DESTINATION_ANGLE, angle);
        deltaX = GET_LONG - P_GetFixedp(po, DMU_START_SPOT_X);
        deltaY = GET_LONG - P_GetFixedp(po, DMU_START_SPOT_Y);
        PO_MovePolyobj(P_GetIntp(po, DMU_TAG), deltaX, deltaY);
        // FIXME: What about speed? It isn't saved at all?
    }
}

//==========================================================================
//
// AssertSegment
//
//==========================================================================

static void AssertSegment(gameArchiveSegment_t segType)
{
    if(GET_LONG != segType)
    {
        Con_Error("Corrupt save game: Segment [%d] failed alignment check",
                  segType);
    }
}

//==========================================================================
//
// ClearSaveSlot
//
// Deletes all save game files associated with a slot number.
//
//==========================================================================

static void ClearSaveSlot(int slot)
{
    int     i;
    char    fileName[100];

    for(i = 0; i < MAX_MAPS; i++)
    {
        sprintf(fileName, "%shex%d%02d.hxs", SavePath, slot, i);
        M_TranslatePath(fileName, fileName);
        remove(fileName);
    }
    sprintf(fileName, "%shex%d.hxs", SavePath, slot);
    M_TranslatePath(fileName, fileName);
    remove(fileName);
}

//==========================================================================
//
// CopySaveSlot
//
// Copies all the save game files from one slot to another.
//
//==========================================================================

static void CopySaveSlot(int sourceSlot, int destSlot)
{
    int     i;
    char    sourceName[100];
    char    destName[100];

    for(i = 0; i < MAX_MAPS; i++)
    {
        sprintf(sourceName, "%shex%d%02d.hxs", SavePath, sourceSlot, i);
        M_TranslatePath(sourceName, sourceName);

        if(ExistingFile(sourceName))
        {
            sprintf(destName, "%shex%d%02d.hxs", SavePath, destSlot, i);
            M_TranslatePath(destName, destName);
            CopyFile(sourceName, destName);
        }
    }
    sprintf(sourceName, "%shex%d.hxs", SavePath, sourceSlot);
    M_TranslatePath(sourceName, sourceName);

    if(ExistingFile(sourceName))
    {
        sprintf(destName, "%shex%d.hxs", SavePath, destSlot);
        M_TranslatePath(destName, destName);
        CopyFile(sourceName, destName);
    }
}

//==========================================================================
//
// CopyFile
//
//==========================================================================

static void CopyFile(char *sourceName, char *destName)
{
    int     length;
    byte   *buffer;
    LZFILE *outf;

    length = M_ReadFile(sourceName, &buffer);
    //gi.WriteFile(destName, buffer, length);
    outf = lzOpen(destName, "wp");
    if(outf)
    {
        lzWrite(buffer, length, outf);
        lzClose(outf);
    }
    Z_Free(buffer);
}

//==========================================================================
//
// ExistingFile
//
//==========================================================================

static boolean ExistingFile(char *name)
{
    FILE   *fp;

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

//==========================================================================
//
// OpenStreamOut
//
//==========================================================================

static void OpenStreamOut(char *fileName)
{
    SavingFP = lzOpen(fileName, "wp");
}

//==========================================================================
//
// CloseStreamOut
//
//==========================================================================

static void CloseStreamOut(void)
{
    if(SavingFP)
    {
        lzClose(SavingFP);
    }
}

//==========================================================================
//
// StreamOutBuffer
//
//==========================================================================

void StreamOutBuffer(void *buffer, int size)
{
    lzWrite(buffer, size, SavingFP);
}

//==========================================================================
//
// StreamOutByte
//
//==========================================================================

void StreamOutByte(byte val)
{
    //fwrite(&val, sizeof(byte), 1, SavingFP);
    lzPutC(val, SavingFP);
}

//==========================================================================
//
// StreamOutWord
//
//==========================================================================
void StreamOutWord(unsigned short val)
{
    //fwrite(&val, sizeof(unsigned short), 1, SavingFP);
    lzPutW(val, SavingFP);
}

//==========================================================================
//
// StreamOutLong
//
//==========================================================================
void StreamOutLong(unsigned int val)
{
    //fwrite(&val, sizeof(int), 1, SavingFP);
    lzPutL(val, SavingFP);
}

//==========================================================================
// StreamOutFloat
//==========================================================================
void StreamOutFloat(float val)
{
    lzPutL(*(int *) &val, SavingFP);
}

//==========================================================================
// SV_Read
//==========================================================================
void SV_Read(void *data, int len)
{
    GET_DATA(data, len);
}

//==========================================================================
// SV_ReadShort
//==========================================================================
short SV_ReadShort(void)
{
    return GET_WORD;
}

//==========================================================================
// SV_ClientSaveGameFile
//==========================================================================
void SV_ClientSaveGameFile(unsigned int game_id, char *str)
{
    // Client heXen Savegame.
    sprintf(str, "%s" CLIENTSAVEGAMENAME "%08X.cxs", DEFAULT_SAVEPATH,
            game_id);
}

//==========================================================================
// SV_LoadClient
//==========================================================================
void SV_LoadClient(unsigned int gameid)
{
    /*  char name[200];
       player_t *cpl = players + consoleplayer;
       mobj_t *mo = cpl->plr->mo;

       if(!IS_CLIENT || !mo) return;

       SV_ClientSaveGameFile(gameid, name);

       // Try to open the file.
       savefile = lzOpen(name, "rp");
       if(!savefile) return;

       SV_Read(&hdr, sizeof(hdr));
       if(hdr.magic != MY_CLIENT_SAVE_MAGIC)
       {
       lzClose(savefile);
       Con_Message("SV_LoadClient: Bad magic!\n");
       return;
       }
       gameskill = hdr.skill;
       deathmatch = hdr.deathmatch;
       nomonsters = hdr.nomonsters;
       respawnparm = hdr.respawn;
       // Do we need to change the map?
       if(gamemap != hdr.map || gameepisode != hdr.episode)
       {
       gamemap = hdr.map;
       gameepisode = hdr.episode;
       G_InitNew(gameskill, gameepisode, gamemap);
       }
       leveltime = hdr.leveltime;

       P_UnsetThingPosition(mo);
       mo->x = SV_ReadLong();
       mo->y = SV_ReadLong();
       mo->z = SV_ReadLong();
       P_SetThingPosition(mo);
       mo->floorz = SV_ReadLong();
       mo->ceilingz = SV_ReadLong();
       mo->angle = cpl->plr->clAngle = SV_ReadLong();
       cpl->plr->clLookDir = SV_ReadFloat();
       SV_ReadPlayer(cpl);

       P_UnArchiveWorld();
       P_UnArchiveSpecials();

       lzClose(savefile); */
}

//==========================================================================
// SV_SaveClient
//==========================================================================
void SV_SaveClient(unsigned int gameid)
{
    /*  char name[200];
       player_t *pl = players + consoleplayer;
       mobj_t *mo = players[consoleplayer].plr->mo;

       if(!IS_CLIENT || !mo) return;

       SV_InitTextureArchives();
       SV_ClientSaveGameFile(gameid, name);

       OpenStreamOut(name);

       StreamOutLong(ASEG_GAME_HEADER);
       StreamOutByte(gamemap);
       StreamOutByte(gameskill);
       StreamOutByte(deathmatch);
       StreamOutByte(nomonsters);
       StreamOutByte(randomclass);

       // Map data.
       StreamOutLong(ASEG_MAP_HEADER);
       StreamOutLong(leveltime); */

    // Our position and look angles.
    /*  StreamOutLong(mo->x);
       StreamOutLong(mo->y);
       StreamOutLong(mo->z);
       StreamOutLong(mo->floorz);
       StreamOutLong(mo->ceilingz);
       StreamOutLong(pl->plr->clAngle);
       StreamOutFLoat(pl->plr->clLookDir);
       ArchivePlayer(players + consoleplayer);

       ArchiveWorld(); */

    /*StreamOutLong(ASEG_END);
       CloseStreamOut(); */

    /*  // Open the file.
       savefile = lzOpen(name, "wp");
       if(!savefile)
       {
       Con_Message("SV_SaveClient: Couldn't open \"%s\" for writing.\n", name);
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
       hdr.respawn = respawnparm;
       hdr.leveltime = leveltime;
       hdr.gameid = gameid;
       SV_Write(&hdr, sizeof(hdr));

       // Some important information.
       // Our position and look angles.
       SV_WriteLong(mo->x);
       SV_WriteLong(mo->y);
       SV_WriteLong(mo->z);
       SV_WriteLong(mo->floorz);
       SV_WriteLong(mo->ceilingz);
       SV_WriteLong(pl->plr->clAngle);
       SV_WriteFloat(pl->plr->clLookDir);
       SV_WritePlayer(consoleplayer);

       P_ArchiveWorld();
       P_ArchiveSpecials();

       lzClose(savefile); */
}
