/**\file p_start.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 */

/**
 * Common player (re)spawning logic.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "p_tick.h"
#include "p_mapsetup.h"
#include "p_user.h"
#include "p_player.h"
#include "d_net.h"
#include "p_map.h"
#include "am_map.h"
#include "p_terraintype.h"
#include "g_common.h"
#include "p_start.h"
#include "p_actor.h"
#include "p_switch.h"
#include "g_defs.h"
#include "p_inventory.h"
#include "p_mapspec.h"
#include "dmu_lib.h"
#include "hu_stuff.h"
#include "hu_chat.h"
#include "r_common.h"

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
#  define TELEPORTSOUND     SFX_TELEPT
#  define MAX_START_SPOTS   4 // Maximum number of different player starts.
#else
#  define TELEPORTSOUND     SFX_TELEPORT
#  define MAX_START_SPOTS   8
#endif

// Time interval for item respawning.
#define SPAWNQUEUE_MAX         128

uint numMapSpots;
mapspot_t* mapSpots;

#if __JHERETIC__
uint maceSpotCount;
mapspotid_t* maceSpots;
uint bossSpotCount;
mapspotid_t* bossSpots;
#endif

static int numPlayerStarts = 0;
static playerstart_t* playerStarts;
static int numPlayerDMStarts = 0;
static playerstart_t* deathmatchStarts;

/**
 * New class (or -1) for each player to be applied when the player respawns.
 * Actually applied on serverside, on the client only valid for the local
 * player(s).
 */
static int playerRespawnAsClass[MAXPLAYERS];

static boolean fuzzySpawnPosition(coord_t* x, coord_t* y, coord_t* z, angle_t* angle,
    int* spawnFlags)
{
#define XOFFSET         (33) // Player radius = 16
#define YOFFSET         (33) // Player radius = 16

    int i;

    assert(x);
    assert(y);

    // Try some spots in the vicinity.
    for(i = 0; i < 9; ++i)
    {
        coord_t pos[2];

        pos[VX] = *x;
        pos[VY] = *y;

        if(i != 0)
        {
            int k = (i == 4 ? 0 : i);

            // Move a bit.
            pos[VX] += (k % 3 - 1) * XOFFSET;
            pos[VY] += (k / 3 - 1) * YOFFSET;
        }

        if(P_CheckSpot(pos[VX], pos[VY]))
        {
            *x = pos[VX];
            *y = pos[VY];
            return true;
        }
    }

    return false;

#undef YOFFSET
#undef XOFFSET
}

void P_ResetPlayerRespawnClasses(void)
{
    int i;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        // No change.
        playerRespawnAsClass[i] = -1;
    }
}

void P_SetPlayerRespawnClass(int plrNum, playerclass_t pc)
{
#ifndef __JHEXEN__
    // There's only one player class.
    assert(pc == PCLASS_PLAYER);
#endif
    playerRespawnAsClass[plrNum] = pc;

#ifdef _DEBUG
    Con_Message("SetPlayerRespawnClass: plrNum=%i class=%i", plrNum, pc);
#endif
}

playerclass_t P_ClassForPlayerWhenRespawning(int plrNum, boolean clear)
{
#if __JHEXEN__
    playerclass_t pClass = cfg.playerClass[plrNum];
#else
    playerclass_t pClass = PCLASS_PLAYER;
#endif

#ifdef _DEBUG
    Con_Message("ClassForPlayerWhenRespawning: plrNum=%i reqclass=%i", plrNum, playerRespawnAsClass[plrNum]);
#endif

    if(playerRespawnAsClass[plrNum] != -1)
    {
        pClass = playerRespawnAsClass[plrNum];
        if(clear)
        {
            // We can now clear the change request.
            playerRespawnAsClass[plrNum] = -1;
        }
    }
#ifdef _DEBUG
    Con_Message("ClassForPlayerWhenRespawning: plrNum=%i actualclass=%i", plrNum, pClass);
#endif

    return pClass;
}

/**
 * Given a doomednum, look up the associated mobj type.
 *
 * @param doomEdNum     Doom Editor (Thing) Number to look up.
 * @return              The associated mobj type if found else @c MT_NONE.
 */
mobjtype_t P_DoomEdNumToMobjType(int doomEdNum)
{
    int i;
    for(i = 0; i < Get(DD_NUMMOBJTYPES); ++i)
    {
        if(doomEdNum == MOBJINFO[i].doomEdNum)
            return i;
    }
    return MT_NONE;
}

void P_Init(void)
{
    P_ResetPlayerRespawnClasses();

    spechit = IterList_New();

#if __JHEXEN__
    X_CreateLUTs();
#endif
#if __JHERETIC__ || __JHEXEN__
    P_InitLava();
#endif

    P_Update();
}

void P_Update(void)
{
#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
    P_InitInventory();
#endif
#if __JHEXEN__
    P_InitMapInfo();
#endif
    P_InitSwitchList();
    P_InitTerrainTypes();

    maxHealth = 100;
    GetDefInt("Player|Max Health", &maxHealth);

#if __JDOOM__ || __JDOOM64__
    healthLimit = 200;
    godModeHealth = 100;
    megaSphereHealth = 200;
    soulSphereHealth = 100;
    soulSphereLimit = 200;

    armorPoints[0] = 100;
    armorPoints[1] = armorPoints[2] = armorPoints[3] = 200;
    armorClass[0] = 1;
    armorClass[1] = armorClass[2] = armorClass[3] = 2;

    GetDefInt("Player|Health Limit", &healthLimit);

    // Previous versions did not feature a separate value for God Health,
    // so if its not found, default to the value of Max Health.
    if(!GetDefInt("Player|God Health", &godModeHealth))
    {
        godModeHealth = maxHealth;
    }

    GetDefInt("Player|Green Armor", &armorPoints[0]);
    GetDefInt("Player|Blue Armor", &armorPoints[1]);
    GetDefInt("Player|IDFA Armor", &armorPoints[2]);
    GetDefInt("Player|IDKFA Armor", &armorPoints[3]);

    GetDefInt("Player|Green Armor Class", &armorClass[0]);
    GetDefInt("Player|Blue Armor Class", &armorClass[1]);
    GetDefInt("Player|IDFA Armor Class", &armorClass[2]);
    GetDefInt("Player|IDKFA Armor Class", &armorClass[3]);

    GetDefInt("MegaSphere|Give|Health", &megaSphereHealth);

    GetDefInt("SoulSphere|Give|Health", &soulSphereHealth);
    GetDefInt("SoulSphere|Give|Health Limit", &soulSphereLimit);
#endif
}

void P_Shutdown(void)
{
    if(spechit)
    {
        IterList_Delete(spechit);
        spechit = 0;
    }

    P_DestroyPlayerStarts();
    P_DestroyAllTagLists();
    P_ShutdownTerrainTypes();
    P_FreeWeaponSlots();
#if __JDOOM__
    P_BrainShutdown();
#endif
}

void P_CreatePlayerStart(int defaultPlrNum, uint entryPoint, boolean deathmatch,
                         mapspotid_t spot)
{
    playerstart_t* start;

    if(deathmatch)
    {
        deathmatchStarts = Z_Realloc(deathmatchStarts,
            sizeof(playerstart_t) * ++numPlayerDMStarts, PU_MAP);
        start = &deathmatchStarts[numPlayerDMStarts - 1];

#ifdef _DEBUG
        Con_Message("P_CreatePlayerStart: DM #%i plrNum=%i entryPoint=%i spot=%i",
                numPlayerDMStarts - 1, defaultPlrNum, entryPoint, spot);
#endif
    }
    else
    {
        playerStarts = Z_Realloc(playerStarts,
            sizeof(playerstart_t) * ++numPlayerStarts, PU_MAP);
        start = &playerStarts[numPlayerStarts - 1];

#ifdef _DEBUG
        Con_Message("P_CreatePlayerStart: Normal #%i plrNum=%i entryPoint=%i spot=%i",
                numPlayerStarts - 1, defaultPlrNum, entryPoint, spot);
#endif
    }

    start->plrNum     = defaultPlrNum;
    start->entryPoint = entryPoint;
    start->spot       = spot;
}

void P_DestroyPlayerStarts(void)
{
    if(playerStarts)
        Z_Free(playerStarts);
    playerStarts = NULL;
    numPlayerStarts = 0;

    if(deathmatchStarts)
        Z_Free(deathmatchStarts);
    deathmatchStarts = NULL;
    numPlayerDMStarts = 0;
}

/**
 * @return  The correct start for the player. The start is in the given
 *          group for specified entry point.
 */
const playerstart_t* P_GetPlayerStart(uint entryPoint, int pnum,
                                      boolean deathmatch)
{
#if __JHEXEN__
    int                 i;
    const playerstart_t* def = NULL;
#endif

    if((deathmatch && !numPlayerDMStarts) || !numPlayerStarts)
        return NULL;

    if(pnum < 0)
        pnum = P_Random() % (deathmatch? numPlayerDMStarts:numPlayerStarts);
    else
        pnum = MINMAX_OF(0, pnum, MAXPLAYERS-1);

    if(deathmatch)
    {
        // In deathmatch, entry point is ignored.
        return &deathmatchStarts[pnum];
    }

#if __JHEXEN__
    // Client 1 should be treated like player 0.
    if(IS_NETWORK_SERVER) pnum--;

    for(i = 0; i < numPlayerStarts; ++i)
    {
        const playerstart_t* start = &playerStarts[i];

        if(start->entryPoint == nextMapEntryPoint && start->plrNum - 1 == pnum)
            return start;
        if(!start->entryPoint && start->plrNum - 1 == pnum)
            def = start;
    }

    // Return the default choice.
    return def;
#else
    return &playerStarts[players[pnum].startSpot];
#endif
}

uint P_GetNumPlayerStarts(boolean deathmatch)
{
    if(deathmatch)
        return numPlayerDMStarts;

    return numPlayerStarts;
}

/**
 * Gives all the players in the game a playerstart.
 * Only needed in co-op games (start spots are random in deathmatch).
 */
void P_DealPlayerStarts(uint entryPoint)
{
    int i;

    if(IS_CLIENT) return;

    if(!numPlayerStarts)
    {
        Con_Message("Warning: Zero player starts found, players will spawn as cameras.");
        return;
    }

    // First assign one start per player, only accepting perfect matches.
    for(i = (IS_NETWORK_SERVER? 1 : 0); i < MAXPLAYERS; ++i)
    {
        int k, spotNumber;
        player_t* pl = &players[i];

        if(!pl->plr->inGame)
            continue;

        // The number of the start spot this player will use.
        spotNumber = i % MAX_START_SPOTS;

        // Player #1 should be treated like #0 on the server.
        if(IS_NETWORK_SERVER) spotNumber--;

        pl->startSpot = -1;

        for(k = 0; k < numPlayerStarts; ++k)
        {
            const playerstart_t* start = &playerStarts[k];

            if(spotNumber == start->plrNum - 1 &&
               start->entryPoint == entryPoint)
            {   // A match!
                pl->startSpot = k;
                // Keep looking.
#ifdef _DEBUG
                Con_Message(" - playerStart %i matches: spot=%i entryPoint=%i",
                            k, spotNumber, entryPoint);
#endif
            }
        }

        // If still without a start spot, assign one randomly.
        if(pl->startSpot == -1)
        {
            // It's likely that some players will get the same start spots.
            pl->startSpot = M_Random() % numPlayerStarts;
        }
    }

    //if(IS_NETGAME)
    {
        Con_Message("Player starting spots:");
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            player_t *pl = &players[i];

            if(!pl->plr->inGame)
                continue;

            Con_Message("- pl%i: color %i, spot %i", i, cfg.playerColor[i],
                       pl->startSpot);
        }
    }
}

/**
 * Called when a player is spawned into the map. Most of the player
 * structure stays unchanged between maps.
 */
void P_SpawnPlayer(int plrNum, playerclass_t pClass, coord_t x, coord_t y, coord_t z,
    angle_t angle, int spawnFlags, boolean makeCamera, boolean pickupItems)
{
    player_t* p;
    mobj_t* mo;

    plrNum = MINMAX_OF(0, plrNum, MAXPLAYERS - 1);

    // Not playing?
    if(!players[plrNum].plr->inGame)
        return;

    pClass = MINMAX_OF(0, pClass, NUM_PLAYER_CLASSES - 1);

    /* $unifiedangles */
    if(!(mo = P_SpawnMobjXYZ(PCLASS_INFO(pClass)->mobjType, x, y, z, angle, spawnFlags)))
        Con_Error("P_SpawnPlayer: Failed spawning mobj for player %i "
                  "(class:%i) pos:[%g, %g, %g] angle:%i.", plrNum, pClass,
                  x, y, z, angle);

#ifdef _DEBUG
    Con_Message("P_SpawnPlayer: Player #%i spawned pos:[%g, %g, %g] angle:%x floorz:%g mobjid:%i",
                plrNum, mo->origin[VX], mo->origin[VY], mo->origin[VZ], mo->angle, mo->floorZ,
                mo->thinker.id);
#endif

    p = &players[plrNum];
    if(p->playerState == PST_REBORN)
        G_PlayerReborn(plrNum);

    /// @todo Should this not occur before the reborn?
    p->class_ = pClass;

    // On clients, mark the remote players.
    if(IS_CLIENT && plrNum != CONSOLEPLAYER)
    {
        mo->ddFlags = DDMF_DONTDRAW;
        // The real flags are received from the server later on.
    }

    // Set color translations for player sprites.
    if(p->colorMap > 0 && p->colorMap < NUMPLAYERCOLORS)
    {
        mo->flags |= p->colorMap << MF_TRANSSHIFT;
    }
    /*
    if(cfg.playerColor[plrNum] > 0)
        mo->flags |= cfg.playerColor[plrNum] << MF_TRANSSHIFT;
        */

#ifdef _DEBUG
    Con_Message("P_SpawnPlayer: Player #%i spawning with color translation %i.",
                plrNum, (mo->flags & MF_TRANSLATION) >> MF_TRANSSHIFT);
#endif

    p->plr->lookDir = 0; /* $unifiedangles */
    p->plr->flags |= DDPF_FIXANGLES | DDPF_FIXORIGIN | DDPF_FIXMOM;
    p->plr->flags &= ~DDPF_UNDEFINED_ORIGIN;
    assert(mo->angle == angle);
    p->jumpTics = 0;
    p->airCounter = 0;
    mo->player = p;
    mo->dPlayer = p->plr;
    mo->health = p->health;

    p->plr->mo = mo;
    p->playerState = PST_LIVE;
    p->refire = 0;
    p->damageCount = 0;
    p->bonusCount = 0;
#if __JHEXEN__
    p->poisonCount = 0;
    p->overridePalette = 0;
#endif
#if __JHERETIC__ || __JHEXEN__
    p->morphTics = 0;
#endif
#if __JHERETIC__
    p->rain1 = NULL;
    p->rain2 = NULL;
#endif
    p->plr->extraLight = 0;
    p->plr->fixedColorMap = 0;

    if(makeCamera)
        p->plr->flags |= DDPF_CAMERA;

    if(p->plr->flags & DDPF_CAMERA)
    {
        VERBOSE( Con_Message("Player #%i spawned as a camera.", plrNum) )

        p->plr->mo->origin[VZ] += (coord_t) cfg.plrViewHeight;
        p->viewHeight = 0;
    }
    else
    {
        p->viewHeight = (coord_t) cfg.plrViewHeight;
    }
    p->viewHeightDelta = 0;

    p->viewZ = p->plr->mo->origin[VZ] + p->viewHeight;
    p->viewOffset[VX] = p->viewOffset[VY] = p->viewOffset[VZ] = 0;

    // Give all cards in death match mode.
    if(deathmatch)
    {
#if __JHEXEN__
        p->keys = 2047;
#else
        int i;
        for(i = 0; i < NUM_KEY_TYPES; ++i)
            p->keys[i] = true;
#endif
    }

    p->pendingWeapon = WT_NOCHANGE;

    if(pickupItems)
    {
        // Check the current position so that any interactions which would
        // occur as a result of collision happen immediately
        // (e.g., weapon pickups at the current position will be collected).
        P_CheckPosition(mo, mo->origin);
    }

    if(p->pendingWeapon != WT_NOCHANGE)
        p->readyWeapon = p->pendingWeapon;
    else
        p->pendingWeapon = p->readyWeapon;

    p->brain.changeWeapon = WT_NOCHANGE;

    p->update |= PSF_READY_WEAPON | PSF_PENDING_WEAPON;

    // Setup gun psprite.
    P_SetupPsprites(p);

    if(!BusyMode_Active())
    {
        /// @todo Is this really necessary after every time a player spawns?
        /// During map setup there are called after the busy mode ends.
        HU_WakeWidgets(p - players);
    }

#if __JHEXEN__
    // Update the player class in effect.
    cfg.playerClass[plrNum] = pClass;
    NetSv_SendPlayerInfo(plrNum, DDSP_ALL_PLAYERS);
    P_ClassForPlayerWhenRespawning(plrNum, true /* now applied; clear change request */);
#endif

    // Player has been spawned, so tell the engine where the camera is
    // initially located. After this it will be updated after every game tick.
    R_UpdateConsoleView(plrNum);
}

static void spawnPlayer(int plrNum, playerclass_t pClass, coord_t x, coord_t y,
    coord_t z, angle_t angle, int spawnFlags, boolean makeCamera, boolean doTeleSpark,
    boolean doTeleFrag)
{
    player_t* plr;
#if __JDOOM__ || __JDOOM64__
    boolean queueBody = (plrNum >= 0? true : false);
#endif
    boolean pickupItems = true;

    /* $voodoodolls */
    if(plrNum < 0)
    {
        plrNum = -plrNum - 1;
        pickupItems = false;
    }
    plrNum = MINMAX_OF(0, plrNum, MAXPLAYERS-1);

    plr = &players[plrNum];

#if __JDOOM__ || __JDOOM64__
    if(queueBody)
        G_QueueBody(plr->plr->mo);
#endif

    P_SpawnPlayer(plrNum, pClass, x, y, z, angle, spawnFlags, makeCamera, pickupItems);

    // Spawn a teleport fog?
    if(doTeleSpark && !makeCamera)
    {
        mobj_t* mo;
        uint an = angle >> ANGLETOFINESHIFT;

        x += 20 * FIX2FLT(finecosine[an]);
        y += 20 * FIX2FLT(finesine[an]);

        if((mo = P_SpawnTeleFog(x, y, angle + ANG180)))
        {
            // Don't start sound on first frame.
            if(mapTime > 1)
                S_StartSound(TELEPORTSOUND, mo);
        }
    }

    // Camera players do not telefrag.
    if(!makeCamera && doTeleFrag)
        P_Telefrag(plr->plr->mo);
}

/**
 * Spawns the client's mobj on clientside.
 */
void P_SpawnClient(int plrNum)
{
    player_t* p;
    playerclass_t pClass = P_ClassForPlayerWhenRespawning(plrNum, true);

#ifdef _DEBUG
    Con_Message("P_SpawnClient: Spawning client player mobj (for player %i; console player is %i).", plrNum, CONSOLEPLAYER);
#endif

    // The server will fix the player's position and angles soon after.
    spawnPlayer(plrNum, pClass, -30000, -30000, 0, 0, MSF_Z_FLOOR, false, false, false);

    p = &players[plrNum];
    p->viewHeight = cfg.plrViewHeight;
    p->viewHeightDelta = 0;

    // The mobj was just spawned onto invalid coordinates. The view cannot
    // be drawn until we receive the right coords.
    p->plr->flags |= DDPF_UNDEFINED_ORIGIN;

    // The weapon of the player is not known. The weapon cannot be raised
    // until we know it.
    p->plr->flags |= DDPF_UNDEFINED_WEAPON;

    // The weapon should be in the down state when spawning.
    p->pSprites[0].pos[VY] = WEAPONBOTTOM;
}

/**
 * Called by G_DoReborn if playing a net game.
 */
void P_RebornPlayerInMultiplayer(int plrNum)
{
#if __JHEXEN__
    int oldKeys = 0, oldPieces = 0, bestWeapon;
    boolean oldWeaponOwned[NUM_WEAPON_TYPES];
#endif
    player_t* p;
    playerclass_t       pClass;

    coord_t pos[3] = { 0, 0, 0 };
    angle_t angle = 0;
    int spawnFlags = 0;
    boolean makeCamera = false;

    if(plrNum < 0 || plrNum >= MAXPLAYERS)
        return; // Wha?

    pClass = P_ClassForPlayerWhenRespawning(plrNum, false);
    p = &players[plrNum];

    Con_Message("P_RebornPlayer: player %i (class %i).", plrNum, pClass);

    if(p->plr->mo)
    {
        // First dissasociate the corpse.
        p->plr->mo->player = NULL;
        p->plr->mo->dPlayer = NULL;
    }

    if(G_GameState() != GS_MAP)
    {
#ifdef _DEBUG
        Con_Message("P_RebornPlayer: Game state is %i, won't spawn.", G_GameState());
#endif
        return; // Nothing else to do.
    }

    // Spawn at random spot if in death match.
    if(deathmatch)
    {
        G_DeathMatchSpawnPlayer(plrNum);
        return;
    }

    // Save player state?
    if(!IS_CLIENT)
    {
#if __JHEXEN__
        int i;

        // Cooperative net-play, retain keys and weapons
        oldKeys = p->keys;
        oldPieces = p->pieces;
        for(i = 0; i < NUM_WEAPON_TYPES; ++i)
            oldWeaponOwned[i] = p->weapons[i].owned;
#endif
    }

    // Determine the spawn position.
    if(IS_CLIENT)
    {
        P_SpawnClient(plrNum);
        return;
    }
    else
    {
        uint entryPoint = gameMapEntryPoint;
        boolean foundSpot = false;
        const playerstart_t* assigned = P_GetPlayerStart(entryPoint, plrNum, false);

        if(assigned)
        {
            const mapspot_t* spot = &mapSpots[assigned->spot];

            if(P_CheckSpot(spot->origin[VX], spot->origin[VY]))
            {
                // Appropriate player start spot is open.
                Con_Printf("- spawning at assigned spot\n");

                pos[VX] = spot->origin[VX];
                pos[VY] = spot->origin[VY];
                pos[VZ] = spot->origin[VZ];
                angle = spot->angle;
                spawnFlags = spot->flags;

                foundSpot = true;
            }
        }

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        if(!foundSpot)
        {
            Con_Message("- force spawning at %i.", p->startSpot);

            if(assigned)
            {
                const mapspot_t* spot = &mapSpots[assigned->spot];

                pos[VX] = spot->origin[VX];
                pos[VY] = spot->origin[VY];
                pos[VZ] = spot->origin[VZ];
                angle = spot->angle;
                spawnFlags = spot->flags;

                // "Fuzz" the spawn position looking for room nearby.
                makeCamera = !fuzzySpawnPosition(&pos[VX], &pos[VY],
                                                 &pos[VZ], &angle,
                                                 &spawnFlags);
            }
            else
            {
                pos[VX] = pos[VY] = pos[VZ] = 0;
                angle = 0;
                spawnFlags = MSF_Z_FLOOR;
                makeCamera = true;
            }
        }
#else
        if(!foundSpot)
        {
            int i;

#ifdef _DEBUG
            Con_Message("P_RebornPlayer: Trying other spots for %i.", plrNum);
#endif

            // Try to spawn at one of the other player start spots.
            for(i = 0; i < MAXPLAYERS; ++i)
            {
                const playerstart_t* start;

                if((start = P_GetPlayerStart(gameMapEntryPoint, i, false)))
                {
                    const mapspot_t* spot = &mapSpots[start->spot];

                    if(P_CheckSpot(spot->origin[VX], spot->origin[VY]))
                    {
                        // Found an open start spot.
                        pos[VX] = spot->origin[VX];
                        pos[VY] = spot->origin[VY];
                        pos[VZ] = spot->origin[VZ];
                        angle = spot->angle;
                        spawnFlags = spot->flags;

                        foundSpot = true;

#ifdef _DEBUG
                        Con_Message("P_RebornPlayer: Spot [%f, %f] selected.", spot->origin[VX], spot->origin[VY]);
#endif
                        break;
                    }
#ifdef _DEBUG
                    Con_Message("P_RebornPlayer: Spot [%f, %f] is not available.", spot->origin[VX], spot->origin[VY]);
#endif
                }
            }
        }

        if(!foundSpot)
        {
            // Player's going to be inside something.
            const playerstart_t* start;

            if((start = P_GetPlayerStart(gameMapEntryPoint, plrNum, false)))
            {
                const mapspot_t* spot = &mapSpots[start->spot];

                pos[VX] = spot->origin[VX];
                pos[VY] = spot->origin[VY];
                pos[VZ] = spot->origin[VZ];
                angle = spot->angle;
                spawnFlags = spot->flags;
            }
            else
            {
                pos[VX] = pos[VY] = pos[VZ] = 0;
                angle = 0;
                spawnFlags = MSF_Z_FLOOR;
                makeCamera = true;
            }
        }
#endif
    }

#ifdef _DEBUG
    Con_Message("P_RebornPlayer: Spawning player at (%f,%f,%f) angle:%x.",
                pos[VX], pos[VY], pos[VZ], angle);
#endif

    spawnPlayer(plrNum, pClass, pos[VX], pos[VY], pos[VZ], angle,
                spawnFlags, makeCamera, true, true);

    DENG_ASSERT(!IS_CLIENT);

    // Restore player state?
#if __JHEXEN__
    {
        int i;

        // Restore keys and weapons
        p->keys = oldKeys;
        p->pieces = oldPieces;
        for(bestWeapon = 0, i = 0; i < NUM_WEAPON_TYPES; ++i)
        {
            if(oldWeaponOwned[i])
            {
                bestWeapon = i;
                p->weapons[i].owned = true;
            }
        }

        GetDefInt("Multiplayer|Reborn|Blue mana",  &p->ammo[AT_BLUEMANA].owned);
        GetDefInt("Multiplayer|Reborn|Green mana", &p->ammo[AT_GREENMANA].owned);

#ifdef _DEBUG
        Con_Message("P_RebornPlayer: Giving mana (b:%i g:%i) to player %i; also old weapons, "
                    "with best weapon %i", p->ammo[AT_BLUEMANA].owned, p->ammo[AT_GREENMANA].owned,
                    plrNum, bestWeapon);
#endif

        if(bestWeapon)
        {
            // Bring up the best weapon.
            p->readyWeapon = p->pendingWeapon = bestWeapon;
        }
    }
#endif
}

boolean P_CheckSpot(coord_t x, coord_t y)
{
#if __JHEXEN__
#define DUMMY_TYPE      MT_PLAYER_FIGHTER
#else
#define DUMMY_TYPE      MT_PLAYER
#endif

    coord_t pos[3];
    mobj_t* dummy;
    boolean result;

    pos[VX] = x;
    pos[VY] = y;
    pos[VZ] = 0;

    // Create a dummy to test with.
    if(!(dummy = P_SpawnMobj(DUMMY_TYPE, pos, 0, MSF_Z_FLOOR)))
        Con_Error("P_CheckSpot: Failed creating dummy mobj.");

    dummy->flags &= ~MF_PICKUP;
    dummy->flags2 &= ~MF2_PASSMOBJ;

    result = P_CheckPosition(dummy, pos);

    P_MobjRemove(dummy, true);

    return result;

#undef DUMMY_TYPE
}

#if __JHERETIC__
void P_AddMaceSpot(mapspotid_t id)
{
    maceSpots = Z_Realloc(maceSpots, sizeof(mapspotid_t) * ++maceSpotCount, PU_MAP);
    maceSpots[maceSpotCount-1] = id;
}

void P_AddBossSpot(mapspotid_t id)
{
    bossSpots = Z_Realloc(bossSpots, sizeof(mapspotid_t) * ++bossSpotCount, PU_MAP);
    bossSpots[bossSpotCount-1] = id;
}
#endif

/**
 * Spawns all players, using the method appropriate for current game mode.
 * Called during map setup.
 */
void P_SpawnPlayers(void)
{
    int                 i;

    if(IS_CLIENT)
    {
        for(i = 0; i < MAXPLAYERS; ++i)
            if(players[i].plr->inGame)
            {
                // Spawn the client anywhere.
                P_SpawnClient(i);
            }
        return;
    }

    // If deathmatch, randomly spawn the active players.
    if(deathmatch)
    {
        for(i = 0; i < MAXPLAYERS; ++i)
            if(players[i].plr->inGame)
            {
                players[i].plr->mo = NULL;
                G_DeathMatchSpawnPlayer(i);
            }
    }
    else
    {
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
        if(!IS_NETGAME)
        {
            /* $voodoodolls */
            for(i = 0; i < numPlayerStarts; ++i)
            {
                if(players[0].startSpot != i && playerStarts[i].plrNum == 1)
                {
                    const mapspot_t* spot = &mapSpots[playerStarts[i].spot];

                    spawnPlayer(-1, PCLASS_PLAYER, spot->origin[VX],
                                spot->origin[VY], spot->origin[VZ],
                                spot->angle, spot->flags, false,
                                false, false);
                }
            }
        }
#endif
        // Spawn everybody at their assigned places.
        // Might get messy if there aren't enough starts.
        for(i = 0; i < MAXPLAYERS; ++i)
            if(players[i].plr->inGame)
            {
                const playerstart_t* start = NULL;
                coord_t pos[3];
                angle_t angle;
                int spawnFlags;
                boolean makeCamera;
                playerclass_t       pClass = P_ClassForPlayerWhenRespawning(i, false);

                if(players[i].startSpot < numPlayerStarts)
                    start = &playerStarts[players[i].startSpot];

                if(start)
                {
                    const mapspot_t* spot = &mapSpots[start->spot];

                    pos[VX] = spot->origin[VX];
                    pos[VY] = spot->origin[VY];
                    pos[VZ] = spot->origin[VZ];
                    angle = spot->angle;
                    spawnFlags = spot->flags;

                    // "Fuzz" the spawn position looking for room nearby.
                    makeCamera = !fuzzySpawnPosition(&pos[VX], &pos[VY],
                        &pos[VZ], &angle, &spawnFlags);
                }
                else
                {
                    pos[VX] = pos[VY] = pos[VZ] = 0;
                    angle = 0;
                    spawnFlags = MSF_Z_FLOOR;
                    makeCamera = true;
                }

                spawnPlayer(i, pClass, pos[VX], pos[VY], pos[VZ], angle,
                            spawnFlags, makeCamera, false, true);

#ifdef _DEBUG
                Con_Message("P_SpawnPlayers: Player %i at %f, %f, %f", i, pos[VX], pos[VY], pos[VZ]);
#endif
            }
    }

    // Let clients know
    for(i = 0; i < MAXPLAYERS; ++i)
        if(players[i].plr->inGame)
        {
            NetSv_SendPlayerSpawnPosition(i, players[i].plr->mo->origin[VX],
                                          players[i].plr->mo->origin[VY],
                                          players[i].plr->mo->origin[VZ],
                                          players[i].plr->mo->angle);
        }
}

/**
 * Spawns a player at one of the random death match spots.
 */
void G_DeathMatchSpawnPlayer(int playerNum)
{
    int                 i;
    playerclass_t       pClass;

    playerNum = MINMAX_OF(0, playerNum, MAXPLAYERS-1);

#if __JHEXEN__
    if(randomClassParm)
    {
        pClass = P_Random() % 3;
        if(pClass == cfg.playerClass[playerNum]) // Not the same class, please.
            pClass = (pClass + 1) % 3;
    }
    else
#endif
    {
        pClass = P_ClassForPlayerWhenRespawning(playerNum, false);
    }

    if(IS_CLIENT)
    {
        if(G_GameState() == GS_MAP)
        {
            // Anywhere will do, for now.
            spawnPlayer(playerNum, pClass, -30000, -30000, 0, 0, MSF_Z_FLOOR, false,
                        false, false);
        }

        return;
    }

    // Now let's find an available deathmatch start.
    if(numPlayerDMStarts < 2)
        Con_Error("G_DeathMatchSpawnPlayer: Error, minimum of two "
                  "(deathmatch) mapspots required for deathmatch.");

#define NUM_TRIES 20
    for(i = 0; i < NUM_TRIES; ++i)
    {
        const mapspot_t* spot = &mapSpots[deathmatchStarts[P_Random() % numPlayerDMStarts].spot];

        // Last attempt will succeed even though blocked.
        if(P_CheckSpot(spot->origin[VX], spot->origin[VY]) || i == NUM_TRIES-1)
        {
            spawnPlayer(playerNum, pClass, spot->origin[VX], spot->origin[VY],
                        spot->origin[VZ], spot->angle, spot->flags, false,
                        true, true);
            return;
        }
    }
}

#if defined(__JHERETIC__) || defined(__JHEXEN__)

typedef struct {
    coord_t pos[2], minDist;
} unstuckmobjinlinedefparams_t;

int unstuckMobjInLinedef(LineDef* li, void* context)
{
    unstuckmobjinlinedefparams_t* params =
        (unstuckmobjinlinedefparams_t*) context;

    if(!P_GetPtrp(li, DMU_BACK_SECTOR))
    {
        coord_t pos, lineOrigin[2], lineDirection[2], result[2];

        /**
         * Project the point (mo position) onto this linedef. If the
         * resultant point lies on the linedef and the current position is
         * in range of that point, adjust the position moving it away from
         * the projected point.
         */

        P_GetDoublepv(P_GetPtrp(li, DMU_VERTEX0), DMU_XY, lineOrigin);
        P_GetDoublepv(li, DMU_DXY, lineDirection);

        pos = V2d_ProjectOnLine(result, params->pos, lineOrigin, lineDirection);

        if(pos > 0 && pos < 1)
        {
            coord_t dist = M_ApproxDistance(params->pos[VX] - result[VX],
                                            params->pos[VY] - result[VY]);

            if(dist >= 0 && dist < params->minDist)
            {
                coord_t len;
                coord_t unit[2], normal[2];

                // Derive the line normal.
                len = M_ApproxDistance(lineDirection[0], lineDirection[1]);
                if(len)
                {
                    unit[VX] = lineDirection[0] / len;
                    unit[VY] = lineDirection[1] / len;
                }
                else
                {
                    unit[VX] = unit[VY] = 0;
                }
                normal[VX] =  unit[VY];
                normal[VY] = -unit[VX];

                // Adjust the position.
                params->pos[VX] += normal[VX] * params->minDist;
                params->pos[VY] += normal[VY] * params->minDist;
            }
        }
    }

    return false; // Continue iteration.
}

typedef struct nearestfacinglineparams_s {
    mobj_t* mo;
    coord_t dist;
    LineDef* line;
} nearestfacinglineparams_t;

static int PIT_FindNearestFacingLine(LineDef* line, void* ptr)
{
    nearestfacinglineparams_t* params = (nearestfacinglineparams_t*) ptr;
    coord_t dist, off;

    dist = LineDef_PointDistance(line, params->mo->origin, &off);
    if(off < 0 || off > P_GetDoublep(line, DMU_LENGTH) || dist < 0)
        return false; // Wrong way or too far.

    if(!params->line || dist < params->dist)
    {
        params->line = line;
        params->dist = dist;
    }

    return false; // Continue.
}

static int turnMobjToNearestLine(thinker_t* th, void* context)
{
    mobj_t* mo = (mobj_t*) th;
    mobjtype_t type = *((mobjtype_t*) context);
    nearestfacinglineparams_t params;
    AABoxd aaBox;

    if(mo->type != type)
        return false; // Continue iteration.

#ifdef _DEBUG
    VERBOSE( Con_Message("Checking mo %i...", mo->thinker.id) );
#endif

    memset(&params, 0, sizeof(params));
    params.mo = mo;

    aaBox.minX = mo->origin[VX] - 50;
    aaBox.minY = mo->origin[VY] - 50;
    aaBox.maxX = mo->origin[VX] + 50;
    aaBox.maxY = mo->origin[VY] + 50;

    VALIDCOUNT++;

    P_LinesBoxIterator(&aaBox, PIT_FindNearestFacingLine, &params);

    if(params.line)
    {
        mo->angle = P_GetAnglep(params.line, DMU_ANGLE) - ANGLE_90;
#ifdef _DEBUG
        VERBOSE( Con_Message("turnMobjToNearestLine: mo=%i angle=%x", mo->thinker.id, mo->angle) );
#endif
    }
    else
    {
#ifdef _DEBUG
        VERBOSE( Con_Message("turnMobjToNearestLine: mo=%i => no nearest line found", mo->thinker.id) );
#endif
    }

    return false; // Continue iteration.
}

static int moveMobjOutOfNearbyLines(thinker_t* th, void* paramaters)
{
    mobj_t* mo = (mobj_t*) th;
    mobjtype_t type = *((mobjtype_t*)paramaters);
    unstuckmobjinlinedefparams_t params;
    AABoxd aaBox;

    // @todo Why not type-prune at an earlier point? We could specify a
    //       custom comparison func for Thinker_Iterate...
    if(mo->type != type)
        return false; // Continue iteration.

    aaBox.minX = mo->origin[VX] - mo->radius;
    aaBox.minY = mo->origin[VY] - mo->radius;
    aaBox.maxX = mo->origin[VX] + mo->radius;
    aaBox.maxY = mo->origin[VY] + mo->radius;

    params.pos[VX] = mo->origin[VX];
    params.pos[VY] = mo->origin[VY];
    params.minDist = mo->radius / 2;

    VALIDCOUNT++;

    P_LinesBoxIterator(&aaBox, unstuckMobjInLinedef, &params);

    if(!FEQUAL(mo->origin[VX], params.pos[VX]) || !FEQUAL(mo->origin[VY], params.pos[VY]))
    {
        P_MobjUnsetOrigin(mo);
        mo->origin[VX] = params.pos[VX];
        mo->origin[VY] = params.pos[VY];
        P_MobjSetOrigin(mo);
    }

    return false; // Continue iteration.
}

/**
 * Only affects torches, which are often placed inside walls in the
 * original maps. The DOOM engine allowed these kinds of things but a
 * Z-buffer doesn't. Also turns the torches so they face the nearest line.
 */
void P_MoveThingsOutOfWalls(void)
{
    static const mobjtype_t types[] = {
#if __JHERETIC__
        MT_MISC10,
#elif __JHEXEN__
        MT_ZWALLTORCH,
        MT_ZWALLTORCH_UNLIT,
#endif
        NUMMOBJTYPES // terminate.
    };
    uint i;

    for(i = 0; types[i] != NUMMOBJTYPES; ++i)
    {
        mobjtype_t type = types[i];
        Thinker_Iterate(P_MobjThinker, moveMobjOutOfNearbyLines, &type);
        Thinker_Iterate(P_MobjThinker, turnMobjToNearestLine, &type);
    }
}

#endif

#ifdef __JHERETIC__
/**
 * Fails in some places, but works most of the time.
 */
void P_TurnGizmosAwayFromDoors(void)
{
#define MAXLIST 200

    Sector* sec;
    mobj_t* iter;
    uint i, l;
    int k, t;
    LineDef* closestline = NULL, *li;
    xline_t* xli;
    coord_t closestdist = 0, dist, off, linelen;
    mobj_t* tlist[MAXLIST];

    for(i = 0; i < numsectors; ++i)
    {
        sec = P_ToPtr(DMU_SECTOR, i);
        memset(tlist, 0, sizeof(tlist));

        // First all the things to process.
        for(k = 0, iter = P_GetPtrp(sec, DMT_MOBJS);
            k < MAXLIST - 1 && iter; iter = iter->sNext)
        {
            if(iter->type == MT_KEYGIZMOBLUE ||
               iter->type == MT_KEYGIZMOGREEN ||
               iter->type == MT_KEYGIZMOYELLOW)
                tlist[k++] = iter;
        }

        // Turn to face away from the nearest door.
        for(t = 0; (iter = tlist[t]) != NULL; ++t)
        {
            closestline = NULL;
            for(l = 0; l < numlines; ++l)
            {
                coord_t d1[2];

                li = P_ToPtr(DMU_LINEDEF, l);

                if(!P_GetPtrp(li, DMU_BACK_SECTOR))
                    continue;

                xli = P_ToXLine(li);

                // It must be a special line with a back sector.
                if((xli->special != 32 && xli->special != 33 &&
                    xli->special != 34 && xli->special != 26 &&
                    xli->special != 27 && xli->special != 28))
                    continue;

                P_GetDoublepv(li, DMU_DXY, d1);
                linelen = M_ApproxDistance(d1[0], d1[1]);

                dist = fabs(LineDef_PointDistance(li, iter->origin, &off));
                if(!closestline || dist < closestdist)
                {
                    closestdist = dist;
                    closestline = li;
                }
            }

            if(closestline)
            {
                Vertex* v0, *v1;
                coord_t v0p[2], v1p[2];

                v0 = P_GetPtrp(closestline, DMU_VERTEX0);
                v1 = P_GetPtrp(closestline, DMU_VERTEX1);

                P_GetDoublepv(v0, DMU_XY, v0p);
                P_GetDoublepv(v1, DMU_XY, v1p);

                iter->angle = M_PointToAngle2(v0p, v1p) - ANG90;
            }
        }
    }
}
#endif
