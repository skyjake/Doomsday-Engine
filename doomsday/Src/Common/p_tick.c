
//**************************************************************************
//**
//** P_TICK.C
//**
//** Top-level tick stuff.
//** Compiles for jDoom, jHeretic or jHexen.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#ifdef __JDOOM__
#  include "d_event.h"
#  include "p_local.h"
#  include "doomstat.h"
#elif __JHERETIC__
#  include "jHeretic/Doomdef.h"
#  include "jHeretic/P_local.h"
#  include "jHeretic/G_game.h"
#elif __JHEXEN__
#  include "jHexen/h2def.h"
#  include "jHexen/p_local.h"
#elif __JSTRIFE__
#  include "jStrife/h2def.h"
#  include "jStrife/p_local.h"
#endif

#include "g_common.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    P_ClientSideThink(void);
void    G_SpecialButton(player_t *pl);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     leveltime;
int     actual_leveltime;
int     TimerGame;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Returns true if the game is currently paused.
 */
boolean P_IsPaused(void)
{
	return paused || (!IS_NETGAME && menuactive);
}

// This is called at all times, no matter gamestate.
void P_RunPlayers(void)
{
	boolean pauseState = P_IsPaused();
	int     i;
	ticcmd_t command;
	boolean gotCommands;

	// This is not for clients.
	if(IS_CLIENT)
		return;

	// Each player gets to think one cmd.
	// For the local player, this is always the cmd of the current tick.
	// For remote players, this might be a predicted cmd or a real cmd
	// from the past.
	for(i = 0; i < MAXPLAYERS; i++)
		if(players[i].plr->ingame)
		{
			// We will combine all the waiting commands into this
			// buffer.
			memset(&command, 0, sizeof(command));
			
			// Get all the commands for the player.
			gotCommands = false;
			while(Net_GetTicCmd(&players[i].cmd, i))
			{
				G_MergeTiccmd(&command, &players[i].cmd);
				gotCommands = true;
			}

			if(gotCommands)
			{
				// The new merged command will be the one that the
				// player uses for thinking on this tick.
				memcpy(&players[i].cmd, &command, sizeof(command));
			}

			// Check for special buttons (pause and netsave).
			G_SpecialButton(&players[i]);

			// The player thinks.
			if(gamestate == GS_LEVEL && !pauseState)
			{
				P_PlayerThink(&players[i]);
			}
		}
}

/*
 * Called 35 times per second. 
 * The heart of play sim.
 */
void P_DoTick(void)
{
	// If the game is paused, nothing will happen.
	if(paused)
		return;

	actual_leveltime++;

#if __JDOOM__
	// pause if in menu and at least one tic has been run
	if(!IS_NETGAME && menuactive && !Get(DD_PLAYBACK) &&
	   players[consoleplayer].plr->viewz != 1)
		return;

#elif __JHERETIC__
	if(!IS_CLIENT && TimerGame && !paused)
	{
		if(!--TimerGame)
			G_ExitLevel();
	}
	// pause if in menu and at least one tic has been run
	if(!IS_NETGAME && menuactive && !Get(DD_PLAYBACK) &&
	   players[consoleplayer].plr->viewz != 1)
		return;

#elif __JHEXEN__
	if(!IS_CLIENT && TimerGame)
	{
		if(!--TimerGame)
			G_Completed(P_TranslateMap(P_GetMapNextMap(gamemap)), 0);
	}
	// pause if in menu and at least one tic has been run
	if(!IS_NETGAME && menuactive && !Get(DD_PLAYBACK) &&
	   players[consoleplayer].plr->viewz != 1)
		return;
#endif

	P_RunThinkers();
	P_UpdateSpecials();

#if __JDOOM__ || __JSTRIFE__
	P_RespawnSpecials();
#elif __JHERETIC__
	P_AmbientSound();
#else
	P_AnimateSurfaces();
#endif

	P_ClientSideThink();

	// For par times, among other things.
	leveltime++;
}
