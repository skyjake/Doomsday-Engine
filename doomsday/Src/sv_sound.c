
//**************************************************************************
//**
//** SV_SOUND.C
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
// Sv_Sound
//===========================================================================
void Sv_Sound(int sound_id, mobj_t *origin, int toPlr)
{
	Sv_SoundAtVolume(sound_id, origin, 127, toPlr);
}

//===========================================================================
// Sv_SoundAtVolume
//	FIXME: Make a new packet, psv_sound2. It should use pretty much the 
//	same parameters as the S_Start/LocalSound routines.
//===========================================================================
void Sv_SoundAtVolume
	(int sound_id_and_flags, mobj_t *origin, int volume, int toPlr)
{
	int sound_id = sound_id_and_flags & ~DDSF_FLAG_MASK;
	int i, flags;
	byte buffer[20], *ptr;
	boolean not;
	int dest;
	int source_sector;

	if(isClient || !sound_id) return;

	// A valid sound ID?
	if(sound_id < 1 || sound_id >= defs.count.sounds.num)
	{
		Con_Message("Sv_SoundAtVolume: Out of bounds ID %i.\n", sound_id);
		return;
	}

	// This is a hack.
	if(sound_id_and_flags & DDSF_NO_ATTENUATION)
		volume = 255;

	ptr = buffer;
	not = (toPlr & SVSF_NOT) != 0;
	dest = toPlr & SVSF_MASK;

	flags = origin ? SNDF_ORIGIN : 0;

	if(origin)
	{
		// Degenmobjs do not have thinker functions.
		if(!origin->thinker.function)
		{
			// Check all sectors; find where the sound is coming from.
			for(i = 0; i < numsectors; i++)
				if(origin == (mobj_t*) &SECTOR_PTR(i)->soundorg)
				{
					flags |= SNDF_SECTOR;
					flags &= ~SNDF_ORIGIN;
					source_sector = i;
					break;
				}
		}
		else
		{
			// Check for player sounds.
			for(i = 0; i < MAXPLAYERS; i++)
				if(players[i].ingame && origin == players[i].mo)
				{
					// Use the flags to specify the console number.
					flags |= SNDF_PLAYER | (i<<4);
					flags &= ~SNDF_ORIGIN;
					break;
				}
		}
	}

	// Should we send volume?
	if(volume != 127)
	{
		if(volume < 0) volume = 0;
		if(volume > 255) volume = 255;
		flags |= SNDF_VOLUME;
	}

	Msg_Begin(psv_sound);
	Msg_WriteByte(flags);
	Msg_WriteByte(sound_id);
	if(flags & SNDF_VOLUME) Msg_WriteByte(volume);
	if(flags & SNDF_SECTOR) Msg_WritePackedShort(source_sector);
	if(flags & SNDF_ORIGIN)
	{
		Msg_WriteShort(origin->x >> 16);
		Msg_WriteShort(origin->y >> 16);
		Msg_WriteShort(origin->z >> 16);
	}
	// Send to the appropriate consoles.
	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(!players[i].ingame || !players[i].mo) continue;
		if(toPlr & SVSF_TO_ALL || (i == dest) != not)
		{
			mobj_t *plrMo = players[i].mo;
			// Check the distance.
			if(origin && P_ApproxDistance(origin->x - plrMo->x, 
				origin->y - plrMo->y) > FRACUNIT * sound_max_distance) 
				continue;
			// We can send to this client.
			Net_SendBuffer(i, 0);
		} 
	}
}
