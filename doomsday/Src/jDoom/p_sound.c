
//**************************************************************************
//**
//** P_SOUND.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "doomdef.h"
#include "doomstat.h"
#include "s_sound.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// S_GetMusicNum
//===========================================================================
int S_GetMusicNum(int episode, int map)
{
	int mnum;

	if (gamemode == commercial)
		mnum = mus_runnin + map - 1;
	else
	{
		int spmus[]=
		{
			// Song - Who? - Where?
			mus_e3m4,	// American		e4m1
			mus_e3m2,	// Romero		e4m2
			mus_e3m3,	// Shawn		e4m3
			mus_e1m5,	// American		e4m4
			mus_e2m7,	// Tim 			e4m5
			mus_e2m4,	// Romero		e4m6
			mus_e2m6,	// J.Anderson	e4m7 CHIRON.WAD
			mus_e2m5,	// Shawn		e4m8
			mus_e1m9	// Tim			e4m9
		};
		
		if (episode < 4)
			mnum = mus_e1m1 + (episode-1)*9 + map-1;
		else
			mnum = spmus[map-1];
	}	
	return mnum;
}

//===========================================================================
// S_LevelMusic
//===========================================================================
void S_LevelMusic(void)
{
	if(gamestate != GS_LEVEL) return;

	// Start new music for the level.
	if(Get(DD_MAP_MUSIC) == -1)
		S_StartMusicNum(S_GetMusicNum(gameepisode, gamemap), true);
	else
		S_StartMusicNum(Get(DD_MAP_MUSIC), true);


//	mus_paused = 0;
/*	if(gi.Get(DD_MAP_MUSIC) == -1)
		S_ChangeMusic(S_GetMusicNum(gameepisode, gamemap), true);
	else
		S_ChangeMusic(gi.Get(DD_MAP_MUSIC), true);*/



//	nextcleanup = 15;
//	listenerSector = NULL;
}

//===========================================================================
// S_SectorSound
//	Doom-like sector sounds: when a new sound starts, stop any old ones
//	from the same origin.
//===========================================================================
void S_SectorSound(sector_t *sec, int id)
{
	mobj_t *origin = (mobj_t*) &sec->soundorg;

	S_StopSound(0, origin);
	S_StartSound(id, origin);
}

