
//**************************************************************************
//**
//** p_tick.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile$
//** $Revision$
//** $Date$
//** $Author$
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "h2def.h"
#include "p_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int leveltime;
int TimerGame;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// P_Ticker
//
//==========================================================================

void P_Ticker(void)
{
	int i;

	if(paused)
	{
		return;
	}

	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(players[i].plr->ingame)
		{
			P_PlayerThink(&players[i]);
		}
	}
	if(TimerGame)
	{
		if(!--TimerGame)
		{
			G_Completed(P_TranslateMap(P_GetMapNextMap(gamemap)), 0);
		}
	}
	gi.RunThinkers();
	P_UpdateSpecials();
	P_AnimateSurfaces();
	leveltime++;
}

