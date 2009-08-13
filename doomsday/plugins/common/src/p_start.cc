/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * p_start.c
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <stdio.h>
#include <string.h>

#if __JDOOM__
#  include "jdoom.h"
#  include "r_common.h"
#  include "hu_stuff.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#  include "r_common.h"
#  include "hu_stuff.h"
#elif __JHERETIC__
#  include "jheretic.h"
#  include "r_common.h"
#  include "hu_stuff.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "p_mapsetup.h"
#include "p_user.h"
#include "d_net.h"
#include "p_map.h"
#include "p_terraintype.h"
#include "g_common.h"
#include "p_start.h"
#include "p_actor.h"
#include "p_switch.h"
#include "g_defs.h"
#include "p_inventory.h"

// MACROS ------------------------------------------------------------------

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
#  define TELEPORTSOUND     SFX_TELEPT
#  define MAX_START_SPOTS   4 // Maximum number of different player starts.
#else
#  define TELEPORTSOUND     SFX_TELEPORT
#  define MAX_START_SPOTS   8
#endif

// Time interval for item respawning.
#define ITEMQUESIZE         128

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Maintain single and multi player starting spots.
spawnspot_t  deathmatchStarts[MAX_DM_STARTS];
spawnspot_t *deathmatchP;

spawnspot_t *playerStarts;
int numPlayerStarts = 0;

spawnspot_t *things;

#if __JHERETIC__
int maceSpotCount;
spawnspot_t *maceSpots;
int bossSpotCount;
spawnspot_t *bossSpots;
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int numPlayerStartsMax = 0;

// CODE --------------------------------------------------------------------

/**
 * Initializes various playsim related data
 */
void P_Init(void)
{
#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
    P_InitInventory();
#endif

#if __JHEXEN__
    P_InitMapInfo();
#endif

    P_InitSwitchList();
    P_InitPicAnims();

    P_InitTerrainTypes();
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    P_InitLava();
#endif

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
    GetDefInt("Player|God Health", &godModeHealth);

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

int P_RegisterPlayerStart(spawnspot_t *mthing)
{
    // Enough room?
    if(++numPlayerStarts > numPlayerStartsMax)
    {
        // Allocate more memory.
        numPlayerStartsMax *= 2;
        if(numPlayerStartsMax < numPlayerStarts)
            numPlayerStartsMax = numPlayerStarts;

        playerStarts = (spawnspot_t*)
            Z_Realloc(playerStarts, sizeof(spawnspot_t) * numPlayerStartsMax, PU_MAP);
    }

    // Copy the properties of this thing
    memcpy(&playerStarts[numPlayerStarts -1], mthing, sizeof(spawnspot_t));
    return numPlayerStarts; // == index + 1
}

void P_FreePlayerStarts(void)
{
    deathmatchP = deathmatchStarts;

    if(playerStarts)
        Z_Free(playerStarts);
    playerStarts = NULL;

    numPlayerStarts = numPlayerStartsMax = 0;
}

/**
 * Gives all the players in the game a playerstart.
 * Only needed in co-op games (start spots are random in deathmatch).
 */
void P_DealPlayerStarts(int group)
{
    int             i, k;
    int             spotNumber;
    player_t       *pl;
    spawnspot_t    *mt;

    if(!numPlayerStarts)
    {
        Con_Message("P_DealPlayerStarts: Warning, no playerstarts found!\n");
        return;
    }

    // First assign one start per player, only accepting perfect matches.
    for(i = 0, pl = players; i < MAXPLAYERS; i++, pl++)
    {
        if(!pl->plr->inGame)
            continue;

        // The number of the start spot this player will use.
        spotNumber = i % MAX_START_SPOTS;
        pl->startSpot = -1;

        for(k = 0, mt = playerStarts; k < numPlayerStarts; ++k, mt++)
        {
            if(spotNumber == mt->type - 1)
            {
                // This is a match.
#if __JHEXEN__
                if(mt->arg1 == group) // Group must match too.
                    pl->startSpot = k;
#else
                pl->startSpot = k;
#endif
                // Keep looking.
            }
        }

        // If still without a start spot, assign one randomly.
        if(pl->startSpot == -1)
        {
            // It's likely that some players will get the same start spots.
            pl->startSpot = M_Random() % numPlayerStarts;
        }
    }

    if(IS_NETGAME)
    {
        Con_Printf("Player starting spots:\n");
        for(i = 0, pl = players; i < MAXPLAYERS; ++i, pl++)
        {
            if(!pl->plr->inGame)
                continue;

            Con_Printf("- pl%i: color %i, spot %i\n", i, cfg.playerColor[i],
                       pl->startSpot);
        }
    }
}

/**
 * Returns false if the player cannot be respawned at the given spawnspot_t
 * spot because something is occupying it
 */
boolean P_CheckSpot(int playernum, spawnspot_t *mthing, boolean doTeleSpark)
{
    float       pos[3];
    ddplayer_t *ddplyr = players[playernum].plr;
    boolean     using_dummy = false;

    pos[VX] = mthing->pos[VX];
    pos[VY] = mthing->pos[VY];

    if(!ddplyr->mo)
    {
        // The player doesn't have a mobj. Let's create a dummy.
        G_DummySpawnPlayer(playernum);
        using_dummy = true;
    }

    ((mobj_t*)ddplyr->mo)->flags2 &= ~MF2_PASSMOBJ;

    if(!P_CheckPosition2f(ddplyr->mo, pos[VX], pos[VY]))
    {
        ((mobj_t*)ddplyr->mo)->flags2 |= MF2_PASSMOBJ;

        if(using_dummy)
        {
            P_MobjRemove(ddplyr->mo, true);
            ddplyr->mo = NULL;
        }
        return false;
    }
    ((mobj_t*)ddplyr->mo)->flags2 |= MF2_PASSMOBJ;

    if(using_dummy)
    {
        P_MobjRemove(ddplyr->mo, true);
        ddplyr->mo = NULL;
    }

#if __JDOOM__ || __JDOOM64__
    G_QueueBody((mobj_t*) ddplyr->mo);
#endif

    if(doTeleSpark) // Spawn a teleport fog
    {
        mobj_t     *mo;
        uint        an = mthing->angle >> ANGLETOFINESHIFT;

        mo = P_SpawnTeleFog(pos[VX] + 20 * FIX2FLT(finecosine[an]),
                            pos[VY] + 20 * FIX2FLT(finesine[an]),
                            mthing->angle + ANG180);

        // Don't start sound on first frame.
        if(players[CONSOLEPLAYER].plr->viewZ != 1)
            S_StartSound(TELEPORTSOUND, mo);
    }

    return true;
}

/**
 * Try to spawn close to the mapspot. Returns false if no clear spot
 * was found.
 */
boolean P_FuzzySpawn(spawnspot_t *spot, int playernum, boolean doTeleSpark)
{
    int         i, k, x, y;
    int         offset = 33; // Player radius = 16
    spawnspot_t place;

    if(spot)
    {
        // Try some spots in the vicinity.
        for(i = 0; i < 9; i++)
        {
            memcpy(&place, spot, sizeof(*spot));

            if(i != 0)
            {
                k = (i == 4 ? 0 : i);
                // Move a bit.
                x = k % 3 - 1;
                y = k / 3 - 1;
                place.pos[VX] += x * offset;
                place.pos[VY] += y * offset;
            }

            if(P_CheckSpot(playernum, &place, doTeleSpark))
            {
                // This is good!
                P_SpawnPlayer(&place, playernum);
                return true;
            }
        }
    }

    // No success. Just spawn at the specified spot.
    P_SpawnPlayer(spot, playernum);

    // Camera players do not collide with the world, so we consider the
    // spot free.
    return (players[playernum].plr->flags & DDPF_CAMERA);
}

#if __JHERETIC__
void P_AddMaceSpot(spawnspot_t *spot)
{
    maceSpots =
        Z_Realloc(maceSpots, sizeof(*spot) * ++maceSpotCount, PU_MAP);
    memcpy(&maceSpots[maceSpotCount-1], spot, sizeof(*spot));
}

void P_AddBossSpot(float x, float y, angle_t angle)
{
    bossSpots = Z_Realloc(bossSpots, sizeof(spawnspot_t) * ++bossSpotCount,
                          PU_MAP);
    bossSpots[bossSpotCount-1].pos[VX] = x;
    bossSpots[bossSpotCount-1].pos[VY] = y;
    bossSpots[bossSpotCount-1].angle = angle;
}
#endif

/**
 * Spawns all THINGS that belong in the map.
 *
 * Polyobject anchors etc are still handled in PO_Init()
 */
void P_SpawnThings(void)
{
    uint            i;
    spawnspot_t    *th;
#if __JDOOM__
    boolean         spawn;
#elif __JHEXEN__
    int             playerCount;
    int             deathSpotsCount;
#endif

#if __JHERETIC__
    maceSpotCount = 0;
    maceSpots = NULL;
    bossSpotCount = 0;
    bossSpots = NULL;
#endif

    for(i = 0; i < numthings; ++i)
    {
        th = &things[i];
        if(DENG2_CLIENT)
        {
            if(th->type < 1 || th->type > 4) continue;
        }
#if __JDOOM__
        // Do not spawn cool, new stuff if !commercial
        spawn = true;
        if(gameMode != commercial)
        {
            switch(th->type)
            {
            case 68:            // Arachnotron
            case 64:            // Archvile
            case 88:            // Boss Brain
            case 89:            // Boss Shooter
            case 69:            // Hell Knight
            case 67:            // Mancubus
            case 71:            // Pain Elemental
            case 74:            // MegaSphere
            case 65:            // Former Human Commando
            case 66:            // Revenant
            case 84:            // Wolf SS
                spawn = false;
                break;
            }
        }
        if(!spawn)
            continue;
#endif
        P_SpawnMapThing(th);
    }

#if __JDOOM__
    if(gameMode == commercial)
        P_SpawnBrainTargets();
#endif

#if __JHERETIC__
    if(maceSpotCount)
    {
        int     spot;

        // Sometimes doesn't show up if not in deathmatch.
        if(!(!deathmatch && P_Random() < 64))
        {
            spot = P_Random() % maceSpotCount;
            P_SpawnMobj3f(MT_WMACE,
                          maceSpots[spot].pos[VX], maceSpots[spot].pos[VY], 0,
                          maceSpots[spot].angle, MTF_Z_FLOOR);
        }
    }
#endif

#if __JHEXEN__
    // \fixme This stuff should be moved!
    P_CreateTIDList();
    P_InitCreatureCorpseQueue(false); // false = do NOT scan for corpses

    if(deathmatch)
    {
        playerCount = 0;
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            if(players[i].plr->inGame)
                playerCount++;
        }

        deathSpotsCount = deathmatchP - deathmatchStarts;
        if(deathSpotsCount < playerCount)
        {
            Con_Error("P_LoadThings: Player count (%d) exceeds deathmatch "
                      "spots (%d)", playerCount, deathSpotsCount);
        }
    }

    // Initialize polyobjs.
    PO_InitForMap();
#endif

    // We're finished with the temporary thing list.
    if(things)
    {
        Z_Free(things);
        things = NULL;
    }
}

/**
 * Spawns all players, using the method appropriate for current game mode.
 * Called during map setup.
 */
void P_SpawnPlayers(void)
{
    int                 i;

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
#if __JDOOM__ || __JDOOM64__
        if(!IS_NETGAME)
        {
            /**
             *\fixme Spawn all unused player starts. This will create 'zombies'.
             * Also in netgames?
             */
            for(i = 0; i < numPlayerStarts; ++i)
            {
                if(players[0].startSpot != i && playerStarts[i].type == 1)
                {
                    P_SpawnPlayer(&playerStarts[i], 0);
                }
            }
        }
#endif
        // Spawn everybody at their assigned places.
        // Might get messy if there aren't enough starts.
        for(i = 0; i < MAXPLAYERS; ++i)
            if(players[i].plr->inGame)
            {
                spawnspot_t    *spot = NULL;
                ddplayer_t     *ddpl = players[i].plr;

                if(players[i].startSpot < numPlayerStarts)
                    spot = &playerStarts[players[i].startSpot];

                if(!P_FuzzySpawn(spot, i, false))
                {
                    // Gib anything at the spot.
                    P_Telefrag(ddpl->mo);
                }
            }
    }
}

/**
 * Spawns the given player at a dummy place.
 */
void G_DummySpawnPlayer(int playernum)
{
    spawnspot_t     faraway;

    memset(&faraway, 0, sizeof(faraway));
    P_SpawnPlayer(&faraway, playernum);
}

/**
 * Spawns a player at one of the random death match spots.
 */
void G_DeathMatchSpawnPlayer(int playerNum)
{
    int                 i = 0, j;
    int                 selections;
    boolean             usingDummy = false;
    ddplayer_t         *pl = players[playerNum].plr;

    // Spawn player initially at a distant location.
    if(!pl->mo)
    {
        G_DummySpawnPlayer(playerNum);
        usingDummy = true;
    }

    // Now let's find an available deathmatch start.
    selections = deathmatchP - deathmatchStarts;
    if(selections < 2)
        Con_Error("Only %i deathmatch spots, 2 required", selections);

    for(j = 0; j < 20; ++j)
    {
        i = P_Random() % selections;
        if(P_CheckSpot(playerNum, &deathmatchStarts[i], true))
        {
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
            deathmatchStarts[i].type = playerNum + 1;
#endif
            break;
        }
    }

    if(usingDummy)
    {
        // Destroy the dummy.
        P_MobjRemove(pl->mo, true);
        pl->mo = NULL;
    }

    P_SpawnPlayer(&deathmatchStarts[i], playerNum);

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    // Gib anything at the spot.
    P_Telefrag(players[playerNum].plr->mo);
#endif
}

/**
 * Returns the correct start for the player. The start is in the given
 * group, or zero if no such group exists.
 *
 * With jDoom groups arn't used at all.
 */
spawnspot_t *P_GetPlayerStart(int group, int pnum)
{
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    return &playerStarts[players[pnum].startSpot];
#else
    int         i;
    spawnspot_t    *mt, *g0choice = NULL;

    for(i = 0; i < numPlayerStarts; ++i)
    {
        mt = &playerStarts[i];

        if(mt->arg1 == group && mt->type - 1 == pnum)
            return mt;
        if(!mt->arg1 && mt->type - 1 == pnum)
            g0choice = mt;
    }

    // Return the group zero choice.
    return g0choice;
#endif
}

typedef struct {
    float               pos[2], minDist;
} unstuckmobjinlinedefparams_t;

boolean unstuckMobjInLinedef(linedef_t* li, void* context)
{
    unstuckmobjinlinedefparams_t *params =
        (unstuckmobjinlinedefparams_t*) context;

    if(!P_GetPtrp(li, DMU_BACK_SECTOR))
    {
        float               pos, linePoint[2], lineDelta[2], result[2];

        /**
         * Project the point (mobj position) onto this linedef. If the
         * resultant point lies on the linedef and the current position is
         * in range of that point, adjust the position moving it away from
         * the projected point.
         */

        P_GetFloatpv(P_GetPtrp(li, DMU_VERTEX0), DMU_XY, linePoint);
        P_GetFloatpv(li, DMU_DXY, lineDelta);

        pos = M_ProjectPointOnLine(params->pos, linePoint, lineDelta, 0, result);

        if(pos > 0 && pos < 1)
        {
            float               dist =
                P_ApproxDistance(params->pos[VX] - result[VX],
                                 params->pos[VY] - result[VY]);

            if(dist >= 0 && dist < params->minDist)
            {
                float               len, unit[2], normal[2];

                // Derive the line normal.
                len = P_ApproxDistance(lineDelta[0], lineDelta[1]);
                if(len)
                {
                    unit[VX] = lineDelta[0] / len;
                    unit[VY] = lineDelta[1] / len;
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

    return true; // Continue iteration.
}

boolean iterateLinedefsNearMobj(thinker_t* th, void* context)
{
    mobj_t*             mo = (mobj_t*) th;
    mobjtype_t          type = *((mobjtype_t*) context);
    float               aabb[4];
    unstuckmobjinlinedefparams_t params;

    // \todo Why not type-prune at an earlier point? We could specify a
    // custom comparison func for DD_IterateThinkers...
    if(mo->type != type)
        return true; // Continue iteration.

    aabb[BOXLEFT]   = mo->pos[VX] - mo->radius;
    aabb[BOXRIGHT]  = mo->pos[VX] + mo->radius;
    aabb[BOXBOTTOM] = mo->pos[VY] - mo->radius;
    aabb[BOXTOP]    = mo->pos[VY] + mo->radius;

    params.pos[VX] = mo->pos[VX];
    params.pos[VY] = mo->pos[VY];
    params.minDist = mo->radius / 2;

    VALIDCOUNT++;

    P_LinesBoxIterator(aabb, unstuckMobjInLinedef, &params);

    if(mo->pos[VX] != params.pos[VX] || mo->pos[VY] != params.pos[VY])
    {
        mo->angle = R_PointToAngle2(mo->pos[VX], mo->pos[VY],
                                    params.pos[VX], params.pos[VY]);
        P_MobjUnsetPosition(mo);
        mo->pos[VX] = params.pos[VX];
        mo->pos[VY] = params.pos[VY];
        P_MobjSetPosition(mo);
    }

    return true; // Continue iteration.
}

/**
 * Only affects torches, which are often placed inside walls in the
 * original maps. The DOOM engine allowed these kinds of things but a
 * Z-buffer doesn't.
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
    uint                i;

    for(i = 0; types[i] != NUMMOBJTYPES; ++i)
    {
        mobjtype_t          type = types[i];

        DD_IterateThinkers((void (*)()) P_MobjThinker, iterateLinedefsNearMobj, &type);
    }
}

#if __JHERETIC__
float P_PointLineDistance(linedef_t *line, float x, float y, float *offset)
{
    float   a[2], b[2], c[2], d[2], len;

    P_GetFloatpv(P_GetPtrp(line, DMU_VERTEX0), DMU_XY, a);
    P_GetFloatpv(P_GetPtrp(line, DMU_VERTEX1), DMU_XY, b);

    c[VX] = x;
    c[VY] = y;

    d[VX] = b[VX] - a[VX];
    d[VY] = b[VY] - a[VY];
    len = sqrt(d[VX] * d[VX] + d[VY] * d[VY]);  // Accurate.

    if(offset)
        *offset =
            ((a[VY] - c[VY]) * (a[VY] - b[VY]) -
             (a[VX] - c[VX]) * (b[VX] - a[VX])) / len;
    return ((a[VY] - c[VY]) * (b[VX] - a[VX]) -
            (a[VX] - c[VX]) * (b[VY] - a[VY])) / len;
}

/**
 * Fails in some places, but works most of the time.
 */
void P_TurnGizmosAwayFromDoors(void)
{
#define MAXLIST 200

    sector_t   *sec;
    mobj_t     *iter;
    uint        i, l;
    int         k, t;
    linedef_t     *closestline = NULL, *li;
    xline_t    *xli;
    float       closestdist = 0, dist, off, linelen;    //, minrad;
    mobj_t     *tlist[MAXLIST];

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
                float               d1[2];

                li = P_ToPtr(DMU_LINEDEF, l);

                if(P_GetPtrp(li, DMU_BACK_SECTOR))
                    continue;

                xli = P_ToXLine(li);

                // It must be a special line with a back sector.
                if((xli->special != 32 && xli->special != 33 &&
                    xli->special != 34 && xli->special != 26 &&
                    xli->special != 27 && xli->special != 28))
                    continue;

                P_GetFloatpv(li, DMU_DXY, d1);
                linelen = P_ApproxDistance(d1[0], d1[1]);

                dist = fabs(P_PointLineDistance(li, iter->pos[VX],
                                                iter->pos[VY], &off));
                if(!closestline || dist < closestdist)
                {
                    closestdist = dist;
                    closestline = li;
                }
            }

            if(closestline)
            {
                vertex_t*       v0, *v1;
                float           v0p[2], v1p[2];

                v0 = P_GetPtrp(closestline, DMU_VERTEX0);
                v1 = P_GetPtrp(closestline, DMU_VERTEX1);

                P_GetFloatpv(v0, DMU_XY, v0p);
                P_GetFloatpv(v1, DMU_XY, v1p);

                iter->angle = R_PointToAngle2(v0p[VX], v0p[VY],
                                              v1p[VX], v1p[VY]) - ANG90;
            }
        }
    }
}
#endif
