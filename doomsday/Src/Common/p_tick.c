
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
#include "p_local.h"
#include "doomstat.h"
#endif

#ifdef __JHERETIC__
#include "Doomdef.h"
#include "P_local.h"
#include "G_game.h"
#endif

#ifdef __JHEXEN__
#include "h2def.h"
#include "p_local.h"
#endif

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void P_ClientSideThink(void);
void G_SpecialButton(player_t *pl);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int leveltime;
int actual_leveltime;
int TimerGame;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Returns true if the game is currently paused.
 */
boolean P_IsPaused(void)
{
#if __JDOOM__
	return paused || (!IS_NETGAME && menuactive);
#else
	return paused;
#endif
}

// This is called at all times, no matter gamestate.
void P_RunPlayers(void)
{
	boolean pausestate = P_IsPaused();
	int	i;

	// This is not for clients.
	if(IS_CLIENT) return;

	// Each player gets to think one cmd.
	// For the local player, this is always the cmd of the current tick.
	// For remote players, this might be a predicted cmd or a real cmd
	// from the past.
	for(i = 0; i < MAXPLAYERS; i++)
		if(players[i].plr->ingame)
		{
			// Get all the commands for the player.
			while(Net_GetTicCmd(&players[i].cmd, i))
			{
				// Check for special buttons (pause and netsave).
				G_SpecialButton(&players[i]);
			
				// The player thinks.
				if(gamestate == GS_LEVEL && !pausestate)
				{
					P_PlayerThink(&players[i]);
				}

				// Local players run one tic at a time.
				if(players[i].plr->flags & DDPF_LOCAL) break;
			}
		}
}

//===========================================================================
// P_DoTick
//	Called 35 times per second. 
//	The heart of play sim.
//===========================================================================
void P_DoTick(void)
{
#if __JDOOM__ || __JHEXEN__
	// If the game is paused, nothing will happen.
    if(paused) return;
#endif

	actual_leveltime++;	

#if __JDOOM__
    // pause if in menu and at least one tic has been run
    if(!IS_NETGAME && 
		menuactive
		&& !Get(DD_PLAYBACK)
		&& players[consoleplayer].plr->viewz != 1) return;

#elif __JHERETIC__
	if(!IS_CLIENT && TimerGame && !paused)
	{
		if(!--TimerGame) G_ExitLevel();
	}
	// If the game is paused, nothing will happen.
    if(paused) return;

#else // __JHEXEN__
	if(!IS_CLIENT && TimerGame)
	{
		if(!--TimerGame)
			G_Completed(P_TranslateMap(P_GetMapNextMap(gamemap)), 0);
	}
#endif

    P_RunThinkers ();
    P_UpdateSpecials ();

#if __JDOOM__
    P_RespawnSpecials ();
#elif __JHERETIC__
	P_AmbientSound();
#elif __JHEXEN__
	P_AnimateSurfaces();
#endif

	P_ClientSideThink();

    // For par times, among other things.
    leveltime++;	
}


