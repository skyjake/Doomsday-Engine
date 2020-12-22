/** @file p_start.cpp  Common player (re)spawning logic.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
 * @authors Copyright © 1993-1996 by id Software, Inc.
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

#include "common.h"
#include "p_start.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <de/nativepath.h>
#include <doomsday/busymode.h>

#include "d_net.h"
#include "d_netsv.h"
#include "dmu_lib.h"
#include "gamesession.h"
#include "g_common.h"
#include "g_defs.h"
#include "hu_stuff.h"
#include "p_actor.h"
#include "p_inventory.h"
#include "p_map.h"
#include "p_mapsetup.h"
#include "p_mapspec.h"
#include "p_switch.h"
#include "p_terraintype.h"
#include "p_tick.h"
#include "p_user.h"
#include "player.h"
#include "r_common.h"

using namespace de;
using namespace common;

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
mapspot_t *mapSpots;

#if __JHERETIC__
uint maceSpotCount;
mapspotid_t *maceSpots;
uint bossSpotCount;
mapspotid_t *bossSpots;
#endif

static int numPlayerStarts = 0;
static playerstart_t *playerStarts;
static int numPlayerDMStarts = 0;
static playerstart_t *deathmatchStarts;

/**
 * New class (or -1) for each player to be applied when the player respawns.
 * Actually applied on serverside, on the client only valid for the local
 * player(s).
 */
static int playerRespawnAsClass[MAXPLAYERS];

static dd_bool fuzzySpawnPosition(coord_t *x, coord_t *y, coord_t *z, angle_t *angle,
    int *spawnFlags)
{
#define XOFFSET         (33) // Player radius = 16
#define YOFFSET         (33) // Player radius = 16

    DE_UNUSED(z, angle, spawnFlags);
    DE_ASSERT(x != 0 && y != 0);

    // Try some spots in the vicinity.
    for(int i = 0; i < 9; ++i)
    {
        coord_t pos[2] = { *x, *y };

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

void P_ResetPlayerRespawnClasses()
{
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        // No change.
        playerRespawnAsClass[i] = -1;
    }
}

void P_SetPlayerRespawnClass(int plrNum, playerclass_t pc)
{
#ifndef __JHEXEN__
    // There's only one player class.
    DE_ASSERT(pc == PCLASS_PLAYER);
#endif
    playerRespawnAsClass[plrNum] = pc;
}

playerclass_t P_ClassForPlayerWhenRespawning(int plrNum, dd_bool clear)
{
#if __JHEXEN__
    playerclass_t pClass = cfg.playerClass[plrNum];
#else
    playerclass_t pClass = PCLASS_PLAYER;
#endif

    if(playerRespawnAsClass[plrNum] != -1)
    {
        pClass = playerclass_t(playerRespawnAsClass[plrNum]);
        if(clear)
        {
            // We can now clear the change request.
            playerRespawnAsClass[plrNum] = -1;
        }
    }

    return pClass;
}

mobjtype_t P_DoomEdNumToMobjType(int doomEdNum)
{
    for(int i = 0; i < Get(DD_NUMMOBJTYPES); ++i)
    {
        if(doomEdNum == MOBJINFO[i].doomEdNum)
            return mobjtype_t(i);
    }
    return MT_NONE;
}

void P_Init()
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

void P_Update()
{
#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
    P_InitInventory();
#endif

    P_InitSwitchList();
    P_InitTerrainTypes();

    ::maxHealth = 100;
    if(const ded_value_t *maxHealth = Defs().getValueById("Player|Max Health"))
    {
        ::maxHealth = String(maxHealth->text).toInt();
    }

#if __JDOOM__ || __JDOOM64__
    ::healthLimit = 200;
    if(const ded_value_t *healthLimit = Defs().getValueById("Player|Health Limit"))
    {
        ::healthLimit = String(healthLimit->text).toInt();
    }

    // Previous versions did not feature a separate value for God Health,
    // so if its not found, default to the value of Max Health.
    ::godModeHealth = ::maxHealth;
    if(const ded_value_t *godHealth = Defs().getValueById("Player|God Health"))
    {
        ::godModeHealth = String(godHealth->text).toInt();
    }

    ::armorPoints[0] = 100;
    if(const ded_value_t *armor = Defs().getValueById("Player|Green Armor"))
    {
        ::armorPoints[0] = String(armor->text).toInt();
    }

    ::armorPoints[1] = 200;
    if(const ded_value_t *armor = Defs().getValueById("Player|Blue Armor"))
    {
        ::armorPoints[1] = String(armor->text).toInt();
    }

    ::armorPoints[2] = 200;
    if(const ded_value_t *armor = Defs().getValueById("Player|IDFA Armor"))
    {
        ::armorPoints[2] = String(armor->text).toInt();
    }

    ::armorPoints[3] = 200;
    if(const ded_value_t *armor = Defs().getValueById("Player|IDKFA Armor"))
    {
        ::armorPoints[3] = String(armor->text).toInt();
    }

    ::armorClass[0] = 1;
    if(const ded_value_t *aclass = Defs().getValueById("Player|Green Armor Class"))
    {
        ::armorClass[0] = String(aclass->text).toInt();
    }

    ::armorClass[1] = 2;
    if(const ded_value_t *aclass = Defs().getValueById("Player|Blue Armor Class"))
    {
        ::armorClass[1] = String(aclass->text).toInt();
    }

    ::armorClass[2] = 2;
    if(const ded_value_t *aclass = Defs().getValueById("Player|IDFA Armor Class"))
    {
        ::armorClass[2] = String(aclass->text).toInt();
    }

    ::armorClass[3] = 2;
    if(const ded_value_t *aclass = Defs().getValueById("Player|IDKFA Armor Class"))
    {
        ::armorClass[3] = String(aclass->text).toInt();
    }

    ::megaSphereHealth = 200;
    if(const ded_value_t *health = Defs().getValueById("MegaSphere|Give|Health"))
    {
        ::megaSphereHealth = String(health->text).toInt();
    }

    ::soulSphereHealth = 100;
    if(const ded_value_t *health = Defs().getValueById("SoulSphere|Give|Health"))
    {
        ::soulSphereHealth = String(health->text).toInt();
    }

    ::soulSphereLimit = 200;
    if(const ded_value_t *healthLimit = Defs().getValueById("SoulSphere|Give|Health Limit"))
    {
        ::soulSphereLimit = String(healthLimit->text).toInt();
    }
#endif
}

void P_Shutdown()
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
    delete theBossBrain; theBossBrain = 0;
#endif
}

void P_CreatePlayerStart(int defaultPlrNum, uint entryPoint, dd_bool deathmatch,
                         mapspotid_t spot)
{
    playerstart_t *start;

    if(deathmatch)
    {
        deathmatchStarts = (playerstart_t *) Z_Realloc(deathmatchStarts, sizeof(playerstart_t) * ++numPlayerDMStarts, PU_MAP);
        start = &deathmatchStarts[numPlayerDMStarts - 1];

        App_Log(DE2_DEV_MAP_VERBOSE, "P_CreatePlayerStart: DM #%i plrNum=%i entryPoint=%i spot=%i",
                numPlayerDMStarts - 1, defaultPlrNum, entryPoint, spot);
    }
    else
    {
        playerStarts = (playerstart_t *) Z_Realloc(playerStarts, sizeof(playerstart_t) * ++numPlayerStarts, PU_MAP);
        start = &playerStarts[numPlayerStarts - 1];

        App_Log(DE2_DEV_MAP_VERBOSE, "P_CreatePlayerStart: Normal #%i plrNum=%i entryPoint=%i spot=%i",
                numPlayerStarts - 1, defaultPlrNum, entryPoint, spot);
    }

    start->plrNum     = defaultPlrNum;
    start->entryPoint = entryPoint;
    start->spot       = spot;
}

void P_DestroyPlayerStarts()
{
    Z_Free(playerStarts); playerStarts = 0;
    numPlayerStarts = 0;

    Z_Free(deathmatchStarts); deathmatchStarts = 0;
    numPlayerDMStarts = 0;
}

const playerstart_t *P_GetPlayerStart(uint entryPoint, int pnum, dd_bool deathmatch)
{
    DE_UNUSED(entryPoint);

    if((deathmatch && !numPlayerDMStarts) || !numPlayerStarts)
        return 0;

    if(pnum < 0)
        pnum = P_Random() % (deathmatch? numPlayerDMStarts:numPlayerStarts);
    else
        pnum = de::clamp(0, pnum, MAXPLAYERS - 1);

    if(deathmatch)
    {
        // In deathmatch, entry point is ignored.
        return &deathmatchStarts[pnum];
    }

#if __JHEXEN__
    // Client 1 should be treated like player 0.
    if(IS_NETWORK_SERVER)
        pnum--;

    const playerstart_t *def = 0;
    for(int i = 0; i < numPlayerStarts; ++i)
    {
        const playerstart_t *start = &playerStarts[i];

        if(start->entryPoint == gfw_Session()->mapEntryPoint() && start->plrNum - 1 == pnum)
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

uint P_GetNumPlayerStarts(dd_bool deathmatch)
{
    return deathmatch? numPlayerDMStarts : numPlayerStarts;
}

void P_DealPlayerStarts(uint entryPoint)
{
    if(IS_CLIENT) return;

    if(!numPlayerStarts)
    {
        App_Log(DE2_MAP_WARNING, "No player starts found, players will spawn as cameras");
        return;
    }

    // First assign one start per player, only accepting perfect matches.
    for(int i = (IS_NETWORK_SERVER? 1 : 0); i < MAXPLAYERS; ++i)
    {
        player_t *pl = &players[i];

        if(!pl->plr->inGame)
            continue;

        // The number of the start spot this player will use.
        int spotNumber = i % MAX_START_SPOTS;

        // Player #1 should be treated like #0 on the server.
        if(IS_NETWORK_SERVER) spotNumber--;

        pl->startSpot = -1;

        for(int k = 0; k < numPlayerStarts; ++k)
        {
            const playerstart_t *start = &playerStarts[k];

            if(spotNumber == start->plrNum - 1 &&
               start->entryPoint == entryPoint)
            {
                // A match!
                pl->startSpot = k;
                // Keep looking.
                App_Log(DE2_DEV_MAP_XVERBOSE, "PlayerStart %i matches: spot=%i entryPoint=%i",
                        k, spotNumber, entryPoint);
            }
        }

        // If still without a start spot, assign one randomly.
        if(pl->startSpot == -1)
        {
            // It's likely that some players will get the same start spots.
            pl->startSpot = M_Random() % numPlayerStarts;
        }
    }

    App_Log(DE2_DEV_MAP_MSG, "Player starting spots:");
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t *pl = &players[i];

        if(!pl->plr->inGame)
            continue;

        App_Log(DE2_DEV_MAP_MSG, "- pl%i: color %i, spot %i", i, cfg.playerColor[i], pl->startSpot);
    }
}

void P_SpawnPlayer(int plrNum, playerclass_t pClass, coord_t x, coord_t y, coord_t z,
    angle_t angle, int spawnFlags, dd_bool makeCamera, dd_bool pickupItems)
{
    plrNum = MINMAX_OF(0, plrNum, MAXPLAYERS - 1);

    // Not playing?
    if(!players[plrNum].plr->inGame)
        return;

    pClass = playerclass_t(MINMAX_OF(0, pClass, NUM_PLAYER_CLASSES - 1));

    /* $unifiedangles */
    mobj_t *mo = P_SpawnMobjXYZ(PCLASS_INFO(pClass)->mobjType, x, y, z, angle, spawnFlags);
    if (!mo)
    {
        Con_Error("P_SpawnPlayer: Failed spawning mobj for player %i "
                  "(class:%i) pos:[%g, %g, %g] angle:%i.", plrNum, pClass,
                  x, y, z, angle);
        return;
    }

    App_Log(DE2_DEV_MAP_MSG, "P_SpawnPlayer: Player #%i spawned pos:(%g, %g, %g) angle:%x floorz:%g mobjid:%i",
            plrNum, mo->origin[VX], mo->origin[VY], mo->origin[VZ], mo->angle, mo->floorZ,
            mo->thinker.id);

    player_t *p = &players[plrNum];
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

    App_Log(DE2_DEV_MAP_VERBOSE, "Player #%i spawning with color translation %i",
            plrNum, (mo->flags & MF_TRANSLATION) >> MF_TRANSSHIFT);

    p->plr->lookDir = 0; /* $unifiedangles */
    p->plr->flags |= DDPF_FIXANGLES | DDPF_FIXORIGIN | DDPF_FIXMOM;
    p->plr->flags &= ~DDPF_UNDEFINED_ORIGIN;
    DE_ASSERT(mo->angle == angle);
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
        App_Log(DE2_MAP_MSG, "Player #%i spawned as a camera", plrNum);

        p->plr->mo->origin[VZ] += (coord_t) cfg.common.plrViewHeight;
        p->viewHeight = 0;
    }
    else
    {
        p->viewHeight = (coord_t) cfg.common.plrViewHeight;
    }
    p->viewHeightDelta = 0;

    p->viewZ = p->plr->mo->origin[VZ] + p->viewHeight;
    p->viewOffset[VX] = p->viewOffset[VY] = p->viewOffset[VZ] = 0;

    // Give all cards in death match mode.
    if(gfw_Rule(deathmatch))
    {
#if __JHEXEN__
        p->keys = 2047;
#else
        for(int i = 0; i < NUM_KEY_TYPES; ++i)
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
        // This is done each time the player spawns so that animations ran at
        // this time are handled correctly (e.g., Hexen's health chain).
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
    coord_t z, angle_t angle, int spawnFlags, dd_bool makeCamera, dd_bool doTeleSpark,
    dd_bool doTeleFrag)
{
#if __JDOOM__ || __JDOOM64__
    dd_bool queueBody = (plrNum >= 0? true : false);
#endif
    dd_bool pickupItems = true;

    /* $voodoodolls */
    if(plrNum < 0)
    {
        plrNum = -plrNum - 1;
        pickupItems = false;
    }
    plrNum = MINMAX_OF(0, plrNum, MAXPLAYERS-1);

    player_t *plr = &players[plrNum];

#if __JDOOM__ || __JDOOM64__
    if(queueBody)
        G_QueueBody(plr->plr->mo);
#endif

    P_SpawnPlayer(plrNum, pClass, x, y, z, angle, spawnFlags, makeCamera, pickupItems);

    // Spawn a teleport fog?
    if(doTeleSpark && !makeCamera)
    {
        uint an = angle >> ANGLETOFINESHIFT;

        x += 20 * FIX2FLT(finecosine[an]);
        y += 20 * FIX2FLT(finesine[an]);

        if(mobj_t *mo = P_SpawnTeleFog(x, y, angle + ANG180))
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
    App_Log(DE2_MAP_VERBOSE,
            "Spawning client player mobj (for player %i; console player is %i)",
            plrNum, CONSOLEPLAYER);

    // The server will fix the player's position and angles soon after.
    spawnPlayer(plrNum, P_ClassForPlayerWhenRespawning(plrNum, true),
                -30000, -30000, 0, 0, MSF_Z_FLOOR, false, false, false);

    player_t *p = &players[plrNum];
    p->viewHeight = cfg.common.plrViewHeight;
    p->viewHeightDelta = 0;

    // The mobj was just spawned onto invalid coordinates. The view cannot
    // be drawn until we receive the right coords.
    p->plr->flags |= DDPF_UNDEFINED_ORIGIN;

    // The weapon of the player is not known. The weapon cannot be raised
    // until we know it.
    p->plr->flags |= DDPF_UNDEFINED_WEAPON;

    // Clear the view filter.
    p->plr->flags &= ~DDPF_USE_VIEW_FILTER;

    // The weapon should be in the down state when spawning.
    p->pSprites[0].pos[VY] = WEAPONBOTTOM;
}

#if __JHEXEN__
static const String &ammoTypeName(int ammoType)
{
    static String const names[NUM_AMMO_TYPES] = {
        /*AT_BLUEMANA*/  "Blue mana",
        /*AT_GREENMANA*/ "Green mana"
    };
    if(ammoType >= AT_FIRST && ammoType < NUM_AMMO_TYPES)
        return names[ammoType - AT_FIRST];
    throw Error("ammoTypeName", "Unknown ammo type " + String::asText(ammoType));
}
#endif  // __JHEXEN__

void P_RebornPlayerInMultiplayer(int plrNum)
{
    if(plrNum < 0 || plrNum >= MAXPLAYERS)
        return; // Wha?

    playerclass_t pClass = P_ClassForPlayerWhenRespawning(plrNum, false);
    player_t *p = &players[plrNum];

    App_Log(DE2_DEV_MAP_MSG, "P_RebornPlayer: player %i (class %i)", plrNum, pClass);

    if(p->plr->mo)
    {
        // First dissasociate the corpse.
        p->plr->mo->player = 0;
        p->plr->mo->dPlayer = 0;
    }

    if(G_GameState() != GS_MAP)
    {
        App_Log(DE2_DEV_MAP_ERROR,
                "P_RebornPlayer: Game state is %i, won't spawn", G_GameState());
        return; // Nothing else to do.
    }

    // Spawn at random spot if in death match.
    if(gfw_Rule(deathmatch))
    {
        G_DeathMatchSpawnPlayer(plrNum);
        return;
    }

    // Save player state?
#if __JHEXEN__
    int oldKeys = 0, oldPieces = 0;
    dd_bool oldWeaponOwned[NUM_WEAPON_TYPES];
    if(!IS_CLIENT)
    {
        // Cooperative net-play, retain keys and weapons
        oldKeys = p->keys;
        oldPieces = p->pieces;
        for(int i = 0; i < NUM_WEAPON_TYPES; ++i)
            oldWeaponOwned[i] = p->weapons[i].owned;
    }
    else
    {
        for (dd_bool &w : oldWeaponOwned) w = false;
    }
#endif

    if(IS_CLIENT)
    {
        P_SpawnClient(plrNum);
        return;
    }

    /*
     * Determine spawn parameters.
     */
    coord_t pos[3] = { 0, 0, 0 };
    angle_t angle = 0;
    int spawnFlags = 0;
    dd_bool makeCamera = false;

    uint entryPoint = gfw_Session()->mapEntryPoint();
    dd_bool foundSpot = false;
    const playerstart_t *assigned = P_GetPlayerStart(entryPoint, plrNum, false);

    if(assigned)
    {
        const mapspot_t *spot = &mapSpots[assigned->spot];

        if(P_CheckSpot(spot->origin[VX], spot->origin[VY]))
        {
            // Appropriate player start spot is open.
            App_Log(DE2_DEV_MAP_MSG, "- spawning at assigned spot");

            pos[VX]    = spot->origin[VX];
            pos[VY]    = spot->origin[VY];
            pos[VZ]    = spot->origin[VZ];
            angle      = spot->angle;
            spawnFlags = spot->flags;

            foundSpot = true;
        }
    }

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    if(!foundSpot)
    {
        App_Log(DE2_DEV_MAP_MSG, "- force spawning at %i", p->startSpot);

        if(assigned)
        {
            const mapspot_t *spot = &mapSpots[assigned->spot];

            pos[VX]    = spot->origin[VX];
            pos[VY]    = spot->origin[VY];
            pos[VZ]    = spot->origin[VZ];
            angle      = spot->angle;
            spawnFlags = spot->flags;

            // "Fuzz" the spawn position looking for room nearby.
            makeCamera = !fuzzySpawnPosition(&pos[VX], &pos[VY], &pos[VZ],
                                             &angle, &spawnFlags);
        }
        else
        {
            pos[VX]    = pos[VY] = pos[VZ] = 0;
            angle      = 0;
            spawnFlags = MSF_Z_FLOOR;
            makeCamera = true;
        }
    }
#else
    if(!foundSpot)
    {
        App_Log(DE2_DEV_MAP_MSG, "P_RebornPlayer: Trying other spots for %i", plrNum);

        // Try to spawn at one of the other player start spots.
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            if(const playerstart_t *start = P_GetPlayerStart(entryPoint, i, false))
            {
                const mapspot_t *spot = &mapSpots[start->spot];

                if(P_CheckSpot(spot->origin[VX], spot->origin[VY]))
                {
                    // Found an open start spot.
                    pos[VX]    = spot->origin[VX];
                    pos[VY]    = spot->origin[VY];
                    pos[VZ]    = spot->origin[VZ];
                    angle      = spot->angle;
                    spawnFlags = spot->flags;

                    foundSpot = true;

                    App_Log(DE2_DEV_MAP_MSG, "P_RebornPlayer: Spot (%g, %g) selected",
                            spot->origin[VX], spot->origin[VY]);
                    break;
                }

                App_Log(DE2_DEV_MAP_VERBOSE, "P_RebornPlayer: Spot (%g, %g) is unavailable",
                        spot->origin[VX], spot->origin[VY]);
            }
        }
    }

    if(!foundSpot)
    {
        // Player's going to be inside something.
        if(const playerstart_t *start = P_GetPlayerStart(entryPoint, plrNum, false))
        {
            const mapspot_t *spot = &mapSpots[start->spot];

            pos[VX]    = spot->origin[VX];
            pos[VY]    = spot->origin[VY];
            pos[VZ]    = spot->origin[VZ];
            angle      = spot->angle;
            spawnFlags = spot->flags;
        }
        else
        {
            pos[VX]    = pos[VY] = pos[VZ] = 0;
            angle      = 0;
            spawnFlags = MSF_Z_FLOOR;
            makeCamera = true;
        }
    }
#endif

    App_Log(DE2_DEV_MAP_NOTE, "Multiplayer-spawning player at (%f,%f,%f) angle:%x",
            pos[VX], pos[VY], pos[VZ], angle);

    spawnPlayer(plrNum, pClass, pos[VX], pos[VY], pos[VZ], angle,
                spawnFlags, makeCamera, true, true);

    DE_ASSERT(!IS_CLIENT);

    // Restore player state?
#if __JHEXEN__
    p->keys = oldKeys;
    p->pieces = oldPieces;
    int bestWeapon = 0;
    for(int i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        if(oldWeaponOwned[i])
        {
            bestWeapon = i;
            p->weapons[i].owned = true;
        }
    }

    for(auto i = int( AT_FIRST ); i < NUM_AMMO_TYPES; ++i)
    {
        if(const ded_value_t *ammo = Defs().getValueById("Multiplayer|Reborn|" + ammoTypeName(i)))
        {
            p->ammo[i].owned = String(ammo->text).toInt();
        }
    }

    App_Log(DE2_MAP_VERBOSE, "Player %i reborn in multiplayer: giving mana (b:%i g:%i); "
            "also old weapons, with best weapon %i", plrNum, p->ammo[AT_BLUEMANA].owned,
            p->ammo[AT_GREENMANA].owned, bestWeapon);

    if(bestWeapon)
    {
        // Bring up the best weapon.
        p->readyWeapon = p->pendingWeapon = weapontype_t(bestWeapon);
    }
#endif
}

dd_bool P_CheckSpot(coord_t x, coord_t y)
{
#if __JHEXEN__
#define DUMMY_TYPE      MT_PLAYER_FIGHTER
#else
#define DUMMY_TYPE      MT_PLAYER
#endif

    // Create a dummy to test with.
    coord_t const pos[3] = { x, y, 0 };
    mobj_t *dummy = P_SpawnMobj(DUMMY_TYPE, pos, 0, MSF_Z_FLOOR);
    DE_ASSERT(dummy);
    if (!dummy) return false; //Con_Error("P_CheckSpot: Failed creating dummy mobj.");

    dummy->flags &= ~MF_PICKUP;
    //dummy->flags2 &= ~MF2_PASSMOBJ;

    dd_bool result = P_CheckPosition(dummy, pos);

    P_MobjRemove(dummy, true);

    return result;

#undef DUMMY_TYPE
}

#if __JHERETIC__
void P_AddMaceSpot(mapspotid_t id)
{
    App_Log(DE2_DEV_MAP_VERBOSE, "P_AddMaceSpot: Added mace spot %u", id);

    maceSpots = (mapspotid_t *)Z_Realloc(maceSpots, sizeof(mapspotid_t) * ++maceSpotCount, PU_MAP);
    maceSpots[maceSpotCount-1] = id;
}

void P_AddBossSpot(mapspotid_t id)
{
    bossSpots = (mapspotid_t *)Z_Realloc(bossSpots, sizeof(mapspotid_t) * ++bossSpotCount, PU_MAP);
    bossSpots[bossSpotCount-1] = id;
}
#endif

void P_SpawnPlayers()
{
    if(IS_CLIENT)
    {
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            if(!players[i].plr->inGame)
                continue;

            // Spawn the client anywhere.
            P_SpawnClient(i);
        }
        return;
    }

    // If deathmatch, randomly spawn the active players.
    if(gfw_Rule(deathmatch))
    {
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            if(!players[i].plr->inGame)
                continue;

            players[i].plr->mo = 0;
            G_DeathMatchSpawnPlayer(i);
        }
    }
    else
    {
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
        if(!IS_NETGAME)
        {
            /* $voodoodolls */
            for(int i = 0; i < numPlayerStarts; ++i)
            {
                if(players[0].startSpot != i && playerStarts[i].plrNum == 1)
                {
                    const mapspot_t *spot = &mapSpots[playerStarts[i].spot];

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
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            player_t *plr = &players[i];

            if(!plr->plr->inGame)
                continue;

            const playerstart_t *start =
                plr->startSpot < numPlayerStarts? &playerStarts[plr->startSpot] : 0;

            coord_t pos[3];
            angle_t angle;
            int spawnFlags;
            dd_bool spawnAsCamera;

            if(start)
            {
                const mapspot_t *spot = &mapSpots[start->spot];

                pos[VX]    = spot->origin[VX];
                pos[VY]    = spot->origin[VY];
                pos[VZ]    = spot->origin[VZ];
                angle      = spot->angle;
                spawnFlags = spot->flags;

                // "Fuzz" the spawn position looking for room nearby.
                spawnAsCamera = !fuzzySpawnPosition(&pos[VX], &pos[VY], &pos[VZ],
                                                    &angle, &spawnFlags);
            }
            else
            {
                pos[VX]    = pos[VY] = pos[VZ] = 0;
                angle      = 0;
                spawnFlags = MSF_Z_FLOOR;

                spawnAsCamera = true;
            }

            spawnPlayer(i, P_ClassForPlayerWhenRespawning(i, false),
                        pos[VX], pos[VY], pos[VZ], angle, spawnFlags,
                        spawnAsCamera, false, true);

            App_Log(DE2_DEV_MAP_MSG, "Player %i spawned at (%g, %g, %g)", i, pos[VX], pos[VY], pos[VZ]);
        }
    }

    // Let clients know
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t *plr = &players[i];
        mobj_t *plMo = plr->plr->mo;

        if(!plr->plr->inGame || !plMo)
            continue;

        NetSv_SendPlayerSpawnPosition(i, plMo->origin[VX], plMo->origin[VY], plMo->origin[VZ],
                                      plMo->angle);
    }
}

void G_DeathMatchSpawnPlayer(int playerNum)
{
    playerNum = MINMAX_OF(0, playerNum, MAXPLAYERS-1);

    playerclass_t pClass;
#if __JHEXEN__
    if (gfw_Rule(randomClasses))
    {
        pClass = playerclass_t(P_Random() % 3);
        if(pClass == cfg.playerClass[playerNum]) // Not the same class, please.
            pClass = playerclass_t((pClass + 1) % 3);
    }
    else
#endif
    {
        pClass = P_ClassForPlayerWhenRespawning(playerNum, false);
    }

    if (IS_CLIENT)
    {
        if (G_GameState() == GS_MAP)
        {
            // Anywhere will do, for now.
            spawnPlayer(playerNum, pClass, -30000, -30000, 0, 0, MSF_Z_FLOOR, false,
                        false, false);
        }
        return;
    }

    // Now let's find an available deathmatch start.
    if (numPlayerDMStarts < 2)
        Con_Error("G_DeathMatchSpawnPlayer: Error, minimum of two "
                  "(deathmatch) mapspots required for deathmatch.");

#define NUM_TRIES 20
    for(int i = 0; i < NUM_TRIES; ++i)
    {
        const mapspot_t *spot = &mapSpots[deathmatchStarts[P_Random() % numPlayerDMStarts].spot];

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

struct unstuckmobjinline_params_t
{
    coord_t pos[2];
    coord_t minDist;
};

/// @return  @c false= continue iteration.
static int unstuckMobjInLine(Line *li, void *context)
{
    unstuckmobjinline_params_t &parm = *static_cast<unstuckmobjinline_params_t *>(context);

    if(!P_GetPtrp(li, DMU_BACK_SECTOR))
    {
        /*
         * Project the point (mo position) onto this line. If the resultant point
         * lies on the line and the current position is in range of that point,
         * adjust the position moving it away from the projected point.
         */

        coord_t lineOrigin[2]; P_GetDoublepv(P_GetPtrp(li, DMU_VERTEX0), DMU_XY, lineOrigin);
        coord_t lineDirection[2]; P_GetDoublepv(li, DMU_DXY, lineDirection);

        coord_t result[2];
        coord_t pos = V2d_ProjectOnLine(result, parm.pos, lineOrigin, lineDirection);

        if(pos > 0 && pos < 1)
        {
            coord_t dist = M_ApproxDistance(parm.pos[VX] - result[VX],
                                            parm.pos[VY] - result[VY]);

            if(dist >= 0 && dist < parm.minDist)
            {
                // Derive the line normal.
                coord_t len = M_ApproxDistance(lineDirection[0], lineDirection[1]);
                coord_t unit[2], normal[2];
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
                parm.pos[VX] += normal[VX] * parm.minDist;
                parm.pos[VY] += normal[VY] * parm.minDist;
            }
        }
    }

    return false; // Continue iteration.
}

struct pit_findnearestfacingline_params_t
{
    mobj_t *mo;
    coord_t dist;
    Line *line;
};

/// @return  @c false= continue iteration.
static int PIT_FindNearestFacingLine(Line *line, void *context)
{
    pit_findnearestfacingline_params_t &parm = *static_cast<pit_findnearestfacingline_params_t *>(context);

    coord_t off;
    coord_t dist = Line_PointDistance(line, parm.mo->origin, &off);

    // Wrong way or too far?
    if(off < 0 || off > P_GetDoublep(line, DMU_LENGTH) || dist < 0)
    {
        return false;
    }

    if(!parm.line || dist < parm.dist)
    {
        parm.line = line;
        parm.dist = dist;
    }

    return false;
}

/// @return  @c false= continue iteration.
static int turnMobjToNearestLine(thinker_t *th, void *context)
{
    mobj_t *mo = reinterpret_cast<mobj_t *>(th);
    mobjtype_t type = *static_cast<mobjtype_t *>(context);

    // @todo Why not type-prune at an earlier point? We could specify a
    //       custom comparison func for Thinker_Iterate...
    if(mo->type != type) return false;

    App_Log(DE2_MAP_XVERBOSE, "Checking mo %i for auto-turning...", mo->thinker.id);

    AABoxd aaBox(mo->origin[VX] - 50, mo->origin[VY] - 50,
                 mo->origin[VX] + 50, mo->origin[VY] + 50);

    pit_findnearestfacingline_params_t parm;
    parm.mo   = mo;
    parm.dist = 0;
    parm.line = 0;

    VALIDCOUNT++;
    Line_BoxIterator(&aaBox, LIF_SECTOR, PIT_FindNearestFacingLine, &parm);

    if(parm.line)
    {
        mo->angle = P_GetAnglep(parm.line, DMU_ANGLE) - ANGLE_90;
        App_Log(DE2_MAP_XVERBOSE, "Turning mobj to nearest line: mo=%i angle=%x",  mo->thinker.id, mo->angle);
    }
    else
    {
        App_Log(DE2_DEV_MAP_XVERBOSE, "Turning mobj to nearest line: mo=%i => no nearest line found",
                mo->thinker.id);
    }

    return false;
}

/// @return  @c false= continue iteration.
static int moveMobjOutOfNearbyLines(thinker_t *th, void *context)
{
    mobj_t *mo = reinterpret_cast<mobj_t *>(th);
    mobjtype_t type = *static_cast<mobjtype_t *>(context);

    // @todo Why not type-prune at an earlier point? We could specify a
    //       custom comparison func for Thinker_Iterate...
    if(mo->type != type) return false;

    AABoxd aaBox(mo->origin[VX] - mo->radius, mo->origin[VY] - mo->radius,
                 mo->origin[VX] + mo->radius, mo->origin[VY] + mo->radius);

    unstuckmobjinline_params_t parm;
    parm.pos[VX] = mo->origin[VX];
    parm.pos[VY] = mo->origin[VY];
    parm.minDist = mo->radius / 2;

    VALIDCOUNT++;
    Line_BoxIterator(&aaBox, LIF_SECTOR, unstuckMobjInLine, &parm);

    if(!FEQUAL(mo->origin[VX], parm.pos[VX]) || !FEQUAL(mo->origin[VY], parm.pos[VY]))
    {
        P_MobjUnlink(mo);
        mo->origin[VX] = parm.pos[VX];
        mo->origin[VY] = parm.pos[VY];
        P_MobjLink(mo);
    }

    return false;
}

void P_MoveThingsOutOfWalls()
{
    static mobjtype_t const types[] = {
#if __JHERETIC__
        MT_MISC10,
#elif __JHEXEN__
        MT_ZWALLTORCH,
        MT_ZWALLTORCH_UNLIT,
#endif
        NUMMOBJTYPES // terminate.
    };

    for(uint i = 0; types[i] != NUMMOBJTYPES; ++i)
    {
        mobjtype_t type = types[i];
        Thinker_Iterate(P_MobjThinker, moveMobjOutOfNearbyLines, &type);
        Thinker_Iterate(P_MobjThinker, turnMobjToNearestLine, &type);
    }
}

#endif

#ifdef __JHERETIC__
void P_TurnGizmosAwayFromDoors()
{
#define MAXLIST 200

    mobj_t *tlist[MAXLIST];

    for(int i = 0; i < numsectors; ++i)
    {
        Sector *sec = (Sector *)P_ToPtr(DMU_SECTOR, i);

        std::memset(tlist, 0, sizeof(tlist));

        // First all the things to process.
        mobj_t *iter = (mobj_t *)P_GetPtrp(sec, DMT_MOBJS);
        for(int k = 0; k < MAXLIST - 1 && iter; iter = iter->sNext)
        {
            if(iter->type == MT_KEYGIZMOBLUE ||
               iter->type == MT_KEYGIZMOGREEN ||
               iter->type == MT_KEYGIZMOYELLOW)
                tlist[k++] = iter;
        }

        // Turn to face away from the nearest door.
        for(int t = 0; (iter = tlist[t]) != NULL; ++t)
        {
            Line *closestline = 0;
            coord_t closestdist = 0;
            for(int l = 0; l < numlines; ++l)
            {
                Line *li = (Line *)P_ToPtr(DMU_LINE, l);

                if(!P_GetPtrp(li, DMU_BACK_SECTOR))
                    continue;

                // It must be a special line with a back sector.
                xline_t *xli = P_ToXLine(li);
                if((xli->special != 32 && xli->special != 33 &&
                    xli->special != 34 && xli->special != 26 &&
                    xli->special != 27 && xli->special != 28))
                    continue;

                coord_t d1[2]; P_GetDoublepv(li, DMU_DXY, d1);

                coord_t off;
                coord_t dist = fabs(Line_PointDistance(li, iter->origin, &off));
                if(!closestline || dist < closestdist)
                {
                    closestdist = dist;
                    closestline = li;
                }
            }

            if(closestline)
            {
                Vertex *v0 = (Vertex *)P_GetPtrp(closestline, DMU_VERTEX0);
                Vertex *v1 = (Vertex *)P_GetPtrp(closestline, DMU_VERTEX1);

                coord_t v0p[2]; P_GetDoublepv(v0, DMU_XY, v0p);
                coord_t v1p[2]; P_GetDoublepv(v1, DMU_XY, v1p);

                iter->angle = M_PointToAngle2(v0p, v1p) - ANG90;
            }
        }
    }
}
#endif
