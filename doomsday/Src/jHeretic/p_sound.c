
//**************************************************************************
//**
//** P_SOUND.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "Doomdef.h"

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
// S_LevelMusic
//	Start the song for the current map.
//===========================================================================
void S_LevelMusic(void)
{
	ddmapinfo_t info;
	char id[10];

	if(gamestate != GS_LEVEL) return;

	sprintf(id, "E%iM%i", gameepisode, gamemap);
	if(Def_Get(DD_DEF_MAP_INFO, id, &info)
		&& info.music >= 0)
	{
		S_StartMusicNum(info.music, true);
	}
	else
	{
		S_StartMusicNum((gameepisode-1)*9 + gamemap-1, true);
	}
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

