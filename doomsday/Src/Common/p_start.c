
//**************************************************************************
//**
//** P_START.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#if __JDOOM__
#include "doomdef.h"
#include "doomstat.h"
#include "p_local.h"
#include "d_config.h"
#include "g_game.h"

#elif __JHERETIC__
#include "doomdef.h"
#include "p_local.h"
#include "settings.h"

#elif __JHEXEN__
#include "h2def.h"
#include "p_local.h"
#include "settings.h"
#endif

#include "d_net.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

mapthing_t	playerstarts[MAXSTARTS], *playerstart_p;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
// P_RegisterPlayerStart
//==========================================================================
void P_RegisterPlayerStart(mapthing_t *mthing)
{
	// Enough room?
	if(playerstart_p-playerstarts >= MAXSTARTS) return;
	*playerstart_p++ = *mthing;	
}

//==========================================================================
// P_DealPlayerStarts
//	Gives all the players in the game a playerstart.
//	Only needed in co-op games (start spots are random in deathmatch).
//==========================================================================
void P_DealPlayerStarts(void)
{
	int			i, k, num = playerstart_p - playerstarts;
	player_t	*pl;
	mapthing_t	*mt;
	int			users[MAXPLAYERS];
	int			fewest;

	if(!num) Con_Error("No playerstarts!\n");

	if(!IS_NETGAME)
	{
		players[0].startspot = 0;
		// In single-player, just find the start for player one.
		for(i = 0, mt = playerstarts; i < num; i++, mt++)
			if(mt->type == 1)
				players[0].startspot = i;
		return;
	}

	// Contains the number of users for each playerstart.
	memset(users, 0, sizeof(users));

	// First assign one start per player, only accepting perfect matches.
	for(i = 0, pl = players; i < MAXPLAYERS; i++, pl++)
	{
		if(!pl->plr->ingame) continue;
		pl->startspot = -1;
		for(k = 0, mt = playerstarts; k < num; k++, mt++)
		{
			if(mt->type-1 == cfg.PlayerColor[i] && !users[k])
			{
				// This is a perfect match.
				users[k]++;
				pl->startspot = k;
				break;
			}
		}	
	}

	// For the left-over players, choose the starts that have the
	// least users.
	for(i = 0, pl = players; i < MAXPLAYERS; i++, pl++)
	{
		if(!pl->plr->ingame || pl->startspot >= 0) continue;
		for(fewest = MAXPLAYERS+1, k = 0; k < num; k++)
			if(users[k] < fewest) 
			{
				fewest = users[k];
				pl->startspot = k;
			}
		users[pl->startspot]++;		
	}
	if(!IS_NETGAME) return;

	Con_Printf( "Player starting spots:\n");
	for(i = 0, pl = players; i < MAXPLAYERS; i++, pl++)
	{
		if(!pl->plr->ingame) continue;
		Con_Printf( "- pl%i: color %i, spot %i\n", i, cfg.PlayerColor[i], 
			pl->startspot);
	}
}

//==========================================================================
// P_SpawnPlayers
//	Spawns all players, using the method appropriate for current game mode.
//	Called during level setup.
//==========================================================================
void P_SpawnPlayers(void)
{
	int i;

    // If deathmatch, randomly spawn the active players.
    if(deathmatch)
    {
		for(i=0 ; i<MAXPLAYERS ; i++)
			if(players[i].plr->ingame)
			{
				players[i].plr->mo = NULL;
				G_DeathMatchSpawnPlayer (i);
			}
    }
	else 
	{
#if __JDOOM__
		if(!IS_NETGAME)
		{
			mapthing_t *it;
			// Spawn all unused player starts.
			// FIXME: Also in netgames?
			for(it = playerstarts; it != playerstart_p; it++)
				if(players[0].startspot != it - playerstarts
					&& it->type == 1)
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
				P_SpawnPlayer(&playerstarts[players[i].startspot], i);
				// Gib anything at the spot.
				if(IS_NETGAME) P_Telefrag(ddpl->mo);
			}
	}
}

#if __JHEXEN__
//==========================================================================
// P_GetPlayerStart
//	Hexen specific. Returns the correct start for the player. The start
//	is in the given group, or zero if no such group exists.
//==========================================================================
mapthing_t *P_GetPlayerStart(int group, int pnum)
{
	mapthing_t *mt, *g0choice = playerstarts;
	
	for(mt=playerstarts; mt<playerstart_p; mt++)
	{
		if(mt->arg1 == group && mt->type-1 == pnum)
			return mt;
		if(!mt->arg1 && mt->type-1 == pnum)
			g0choice = mt;
	}
	// Return the group zero choice.
	return g0choice;
}
#endif