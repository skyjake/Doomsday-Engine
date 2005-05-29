
//**************************************************************************
//**
//** P_START.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#if __JDOOM__
#  include "doomdef.h"
#  include "doomstat.h"
#  include "p_local.h"
#  include "d_config.h"
#  include "g_game.h"
#  include "s_sound.h"
#elif __JHERETIC__
#  include "jHeretic/Doomdef.h"
#  include "jHeretic/P_local.h"
#  include "jHeretic/d_config.h"
#elif __JHEXEN__
#  include "jHexen/h2def.h"
#  include "jHexen/p_local.h"
#  include "jHexen/d_config.h"
#elif __JSTRIFE__
#  include "jStrife/h2def.h"
#  include "jStrife/p_local.h"
#  include "jStrife/d_config.h"
#endif

#include "d_net.h"

// MACROS ------------------------------------------------------------------

// Maximum number of different player starts.
#if __JDOOM__ || __JHERETIC__
#  define MAX_START_SPOTS 4
#else
#  define MAX_START_SPOTS 8
#endif

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

mapthing_t playerstarts[MAXSTARTS], *playerstart_p;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
// P_RegisterPlayerStart
//==========================================================================
void P_RegisterPlayerStart(mapthing_t * mthing)
{
	// Enough room?
	if(playerstart_p - playerstarts >= MAXSTARTS)
		return;
	*playerstart_p++ = *mthing;
}

//==========================================================================
// P_DealPlayerStarts
//  Gives all the players in the game a playerstart.
//  Only needed in co-op games (start spots are random in deathmatch).
//==========================================================================
void P_DealPlayerStarts(void)
{
	int     i, k, num = playerstart_p - playerstarts;
	player_t *pl;
	mapthing_t *mt;
	int     spotNumber;

	if(!num)
		Con_Error("No playerstarts!\n");

	// First assign one start per player, only accepting perfect matches.
	for(i = 0, pl = players; i < MAXPLAYERS; i++, pl++)
	{
		if(!pl->plr->ingame)
			continue;

		// The number of the start spot this player will use.
		spotNumber = i % MAX_START_SPOTS;
		pl->startspot = -1;

		for(k = 0, mt = playerstarts; k < num; k++, mt++)
		{
			if(spotNumber == mt->type - 1)
			{
				// This is a match.
				pl->startspot = k;
				// Keep looking.
			}
		}

		// If still without a start spot, assign one randomly.
		if(pl->startspot == -1)
		{
			// It's likely that some players will get the same start
			// spots.
			pl->startspot = M_Random() % num;
		}
	}

	if(IS_NETGAME)
	{
		Con_Printf("Player starting spots:\n");
		for(i = 0, pl = players; i < MAXPLAYERS; i++, pl++)
		{
			if(!pl->plr->ingame)
				continue;
			Con_Printf("- pl%i: color %i, spot %i\n", i, cfg.PlayerColor[i],
					   pl->startspot);
		}
	}
}

//===========================================================================
// P_CheckSpot
//  Returns false if the player cannot be respawned
//  at the given mapthing_t spot because something is occupying it 
//  FIXME: Quite a mess!
//===========================================================================
boolean P_CheckSpot(int playernum, mapthing_t * mthing, boolean doTeleSpark)
{
	fixed_t x;
	fixed_t y;
	unsigned an;
	mobj_t *mo;

#if __JDOOM__ || __JHEXEN__ || __JSTRIFE__
	subsector_t *ss;
#endif
#if __JDOOM__ || __JSTRIFE__
	int     i;
#endif
#if __JHERETIC__ || __JHEXEN__
	mapthing_t faraway;
	boolean using_dummy = false;
#endif

#if __JDOOM__ || __JSTRIFE__
	if(!players[playernum].plr->mo)
	{
		// first spawn of level, before corpses
		for(i = 0; i < playernum; i++)
		{
			if(players[i].plr->mo &&
			   players[i].plr->mo->x == mthing->x << FRACBITS &&
			   players[i].plr->mo->y == mthing->y << FRACBITS)
				return false;
		}
		return true;
	}
#endif

	x = mthing->x << FRACBITS;
	y = mthing->y << FRACBITS;

#if __JHERETIC__ || __JHEXEN__
	if(!players[playernum].plr->mo)
	{
		// The player doesn't have a mobj. Let's create a dummy.
		faraway.x = faraway.y = DDMAXSHORT;
		P_SpawnPlayer(&faraway, playernum);
		using_dummy = true;
	}
	players[playernum].plr->mo->flags2 &= ~MF2_PASSMOBJ;
#endif

	if(!P_CheckPosition(players[playernum].plr->mo, x, y))
	{
#if __JHERETIC__ || __JHEXEN__
		players[playernum].plr->mo->flags2 |= MF2_PASSMOBJ;
		if(using_dummy)
		{
			P_RemoveMobj(players[playernum].plr->mo);
			players[playernum].plr->mo = NULL;
		}
#endif
		return false;
	}

#if __JHERETIC__
	players[playernum].plr->mo->flags2 |= MF2_PASSMOBJ;
#endif

#if __JHERETIC__ || __JHEXEN__
	if(using_dummy)
	{
		P_RemoveMobj(players[playernum].plr->mo);
		players[playernum].plr->mo = NULL;
	}
#endif

#if __JDOOM__
	G_QueueBody(players[playernum].plr->mo);
#endif

	if(doTeleSpark)
	{
		// spawn a teleport fog 
		an = (ANG45 * (mthing->angle / 45)) >> ANGLETOFINESHIFT;

#if __JDOOM__ || __JHEXEN__ || __JSTRIFE__
		ss = R_PointInSubsector(x, y);
		mo = P_SpawnMobj(x + 20 * finecosine[an], y + 20 * finesine[an],
						 ss->sector->floorheight, MT_TFOG);
#else							// __JHERETIC__
		mo = P_SpawnTeleFog(x + 20 * finecosine[an], y + 20 * finesine[an]);
#endif

		// don't start sound on first frame
		if(players[consoleplayer].plr->viewz != 1)
		{
#if __JHEXEN__ || __JSTRIFE__
			S_StartSound(SFX_TELEPORT, mo);
#else
			S_StartSound(sfx_telept, mo);
#endif
		}
	}

	return true;
}

//===========================================================================
// P_FuzzySpawn
//  Try to spawn close to the mapspot. Returns false if no clear spot 
//  was found.
//===========================================================================
boolean P_FuzzySpawn(mapthing_t * spot, int playernum, boolean doTeleSpark)
{
	int     i, k, x, y;
	int     offset = 33;		// Player radius = 16
	mapthing_t place;

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
			place.x += x * offset;
			place.y += y * offset;
		}
		if(P_CheckSpot(playernum, &place, doTeleSpark))
		{
			// This is good!
			P_SpawnPlayer(&place, playernum);
			return true;
		}
	}

	// No success. Just spawn at the specified spot.
	P_SpawnPlayer(spot, playernum);
	return false;
}

//==========================================================================
// P_SpawnPlayers
//  Spawns all players, using the method appropriate for current game mode.
//  Called during level setup.
//==========================================================================
void P_SpawnPlayers(void)
{
	int     i;

	// If deathmatch, randomly spawn the active players.
	if(deathmatch)
	{
		for(i = 0; i < MAXPLAYERS; i++)
			if(players[i].plr->ingame)
			{
				players[i].plr->mo = NULL;
				G_DeathMatchSpawnPlayer(i);
			}
	}
	else
	{
#ifdef __JDOOM__
		if(!IS_NETGAME)
		{
			mapthing_t *it;

			// Spawn all unused player starts. This will create 'zombies'.
			// FIXME: Also in netgames?
			for(it = playerstarts; it != playerstart_p; it++)
				if(players[0].startspot != it - playerstarts && it->type == 1)
				{
					P_SpawnPlayer(it, 0);
				}
		}
#endif
		// Spawn everybody at their assigned places.
		// Might get messy if there aren't enough starts.
		for(i = 0; i < MAXPLAYERS; i++)
			if(players[i].plr->ingame)
			{
				ddplayer_t *ddpl = players[i].plr;

				if(!P_FuzzySpawn
				   (&playerstarts[players[i].startspot], i, false))
				{
					// Gib anything at the spot.
					P_Telefrag(ddpl->mo);
				}
			}
	}
}

#if __JHEXEN__ || __JSTRIFE__
//==========================================================================
// P_GetPlayerStart
//  Hexen specific. Returns the correct start for the player. The start
//  is in the given group, or zero if no such group exists.
//==========================================================================
mapthing_t *P_GetPlayerStart(int group, int pnum)
{
	mapthing_t *mt, *g0choice = playerstarts;

	for(mt = playerstarts; mt < playerstart_p; mt++)
	{
		if(mt->arg1 == group && mt->type - 1 == pnum)
			return mt;
		if(!mt->arg1 && mt->type - 1 == pnum)
			g0choice = mt;
	}
	// Return the group zero choice.
	return g0choice;
}
#endif

/*
 * Compose the name of the map lump identifier.
 */
void P_GetMapLumpName(int episode, int map, char *lumpName)
{
#ifdef __JDOOM__
	if(gamemode == commercial)
	{
		sprintf(lumpName, "map%02i", map);
	}
	else
	{
		sprintf(lumpName, "E%iM%i", episode, map);
	}
#endif

#ifdef __JHERETIC__
	sprintf(lumpName, "E%iM%i", episode, map);
#endif

#if __JHEXEN__ || __JSTRIFE__
	sprintf(lumpName, "MAP%02i", map);
#endif
}

/*
 * Locate the lump indices where the data of the specified map
 * resides.
 */
void P_LocateMapLumps(int episode, int map, int *lumpIndices)
{
	char lumpName[40];
	char glLumpName[40];
	
	// Find map name.
	P_GetMapLumpName(episode, map, lumpName);
	sprintf(glLumpName, "GL_%s", lumpName);
	Con_Message("SetupLevel: %s\n", lumpName);

	// Let's see if a plugin is available for loading the data.
	if(!Plug_DoHook(HOOK_LOAD_MAP_LUMPS, W_GetNumForName(lumpName),
					(void*) lumpIndices))
	{
		// The plugin failed.
		lumpIndices[0] = W_GetNumForName(lumpName);
		lumpIndices[1] = W_CheckNumForName(glLumpName);
	}
}
