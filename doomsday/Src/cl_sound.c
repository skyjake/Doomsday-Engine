
//**************************************************************************
//**
//** CL_SOUND.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"
#include "de_audio.h"

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
// Cl_Sound
//	Called when a sound packet is received.
//===========================================================================
void Cl_Sound(void)
{
	int sound, volume = 127;
	float pos[3];
	byte flags;
	int num;
	mobj_t *mo = NULL;

	flags = Msg_ReadByte();
	sound = Msg_ReadByte();
	if(sound < 1 || sound >= defs.count.sounds.num)
	{
		Con_Message("Cl_Sound: Out of bounds ID %i.\n", sound);
		return; // Bad sound ID!
	}
	if(flags & SNDF_VOLUME) 
	{
		volume = Msg_ReadByte();
		if(volume > 127)
		{
			volume = 127;
			sound |= DDSF_NO_ATTENUATION;
		}
	}
	if(flags & SNDF_SECTOR) 
	{
		num = Msg_ReadPackedShort();
		if(num < 0 || num >= numsectors)
		{
			Con_Message("Cl_Sound: Invalid sector number %i.\n", num);
			return;
		}
		mo = (mobj_t*) &SECTOR_PTR(num)->soundorg;
		S_StopSound(0, mo);
		S_LocalSoundAtVolume(sound, mo, volume/127.0f);
	}
	else if(flags & SNDF_ORIGIN)
	{
		pos[VX] = Msg_ReadShort();
		pos[VY] = Msg_ReadShort();
		pos[VZ] = Msg_ReadShort();
		S_LocalSoundAtVolumeFrom(sound, NULL, pos, volume/127.0f);
	}
	else if(flags & SNDF_PLAYER)
	{
		S_LocalSoundAtVolume(sound, 
			players[(flags & 0xf0) >> 4].mo, 
			volume/127.0f);
	}
	else 
	{
		// Play it from "somewhere".
		S_LocalSoundAtVolume(sound, NULL, volume/127.0f);
	}
}

