 
//**************************************************************************
//**
//** S_SOUND.C
//**
//** Version:		1.0
//** Last Build:	-?-
//** Author:		jk
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "h2def.h"
#include "r_local.h"
#include "p_local.h"    // for P_ApproxDistance
#include "sounds.h"
#include "settings.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
// S_GetSoundID
//==========================================================================
int S_GetSoundID(char *name)
{
	return Def_Get(DD_DEF_SOUND_BY_NAME, name, 0);
}

//===========================================================================
// S_LevelMusic
//	Starts the song of the current level.
//===========================================================================
void S_LevelMusic(void)
{
	int idx = Def_Get(DD_DEF_MUSIC, "currentmap", 0);
	
	// Update the 'currentmap' music definition.
	Def_Set(DD_DEF_MUSIC, idx, DD_LUMP, P_GetMapSongLump(gamemap));
	Def_Set(DD_DEF_MUSIC, idx, DD_CD_TRACK, (void*) P_GetMapCDTrack(gamemap));
	S_StartMusic("currentmap", true);
}

//==========================================================================
// S_InitScript
//==========================================================================
void S_InitScript(void)
{
	int i;
	char buf[80];

	strcpy(ArchivePath, DEFAULT_ARCHIVEPATH);
	SC_OpenLump("SNDINFO");
	while(SC_GetString())
	{
		if(*sc_String == '$')
		{
			if(!stricmp(sc_String, "$ARCHIVEPATH"))
			{
				SC_MustGetString();
				//strcpy(ArchivePath, sc_String);
			}
			else if(!stricmp(sc_String, "$MAP"))
			{
				SC_MustGetNumber();
				SC_MustGetString();
				if(sc_Number)
				{
					P_PutMapSongLump(sc_Number, sc_String);
				}
			}
			continue;
		}
		else
		{
			if((i = Def_Get(DD_DEF_SOUND_BY_NAME, sc_String, 0)))
			{
				SC_MustGetString();
				Def_Set(DD_DEF_SOUND, i, DD_LUMP, 
					*sc_String != '?'? sc_String : "default");
			}
			else
			{
				// Read the lumpname anyway.
				SC_MustGetString();
			}
		}
	}
	SC_Close();

	// All sounds left without a lumpname will use "DEFAULT".
	for(i = 0; i < Get(DD_NUMSOUNDS); i++)
	{
		Def_Get(DD_DEF_SOUND_LUMPNAME, (char*) i, buf);
		if(!strcmp(buf, ""))
			Def_Set(DD_DEF_SOUND, i, DD_LUMP, "default");
	}
}



