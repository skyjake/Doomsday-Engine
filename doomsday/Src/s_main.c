/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * s_main.c: Sound Subsystem
 *
 * Interface to the Sfx and Mus modules. 
 * High-level (and exported) sound control.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"

#include "r_extres.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int			sound_info = false;
int			sound_min_distance = 256; // No distance attenuation this close.
int			sound_max_distance = 2025;

// Setting these variables is enough to adjust the volumes.
// S_StartFrame() will call the actual routines to change the volume
// when there are changes.
int			sfx_volume = 255, mus_volume = 255;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean nopitch;

// CODE --------------------------------------------------------------------

//===========================================================================
// S_Init
//	Main sound system initialization. Inits both the Sfx and Mus modules.
//	Returns true if there were no errors.
//===========================================================================
boolean S_Init(void)
{
	boolean sfx_ok, mus_ok;

	if(ArgExists("-nosound")) return true;

	// Disable random pitch changes?
	nopitch = ArgExists("-nopitch");

	sfx_ok = Sfx_Init();
	mus_ok = Mus_Init();

	Con_Message("S_Init: %s.\n", sfx_ok && mus_ok? "OK"
		: "Errors during initialization.");
	return (sfx_ok && mus_ok);
}

//===========================================================================
// S_Shutdown
//	Shutdown the whole sound system (Sfx + Mus).
//===========================================================================
void S_Shutdown(void)
{
	Sfx_Shutdown();
	Mus_Shutdown();
}

//===========================================================================
// S_LevelChange
//	Must be called before the level is changed.
//===========================================================================
void S_LevelChange(void)
{
	// Stop everything in the LSM.
	Sfx_InitLogical();

	Sfx_LevelChange();
}

//===========================================================================
// S_Reset
//	Stop all channels and music, delete the entire sample cache.
//===========================================================================
void S_Reset(void)
{
	Sfx_Reset();
	S_StopMusic();
}

//===========================================================================
// S_StartFrame
//===========================================================================
void S_StartFrame(void)
{
	static int old_mus_volume = -1;

	if(mus_volume != old_mus_volume)
	{
		old_mus_volume = mus_volume;
		Mus_SetVolume(mus_volume / 255.0f);
	}

	// Update all channels (freq, 2D:pan,volume, 3D:position,velocity).
	Sfx_StartFrame();
	Mus_StartFrame();

	// Remove stopped sounds from the LSM.
	Sfx_PurgeLogical();
}

//===========================================================================
// S_EndFrame
//===========================================================================
void S_EndFrame(void)
{
	Sfx_EndFrame();
}

//===========================================================================
// S_GetListenerMobj
//	Usually the display player.
//===========================================================================
mobj_t *S_GetListenerMobj(void)
{
	return players[displayplayer].mo;
}

//===========================================================================
// S_GetSoundInfo
//	freq and volume may be NULL. They will be modified by sound links.
//===========================================================================
sfxinfo_t *S_GetSoundInfo(int sound_id, float *freq, float *volume)
{
	float dummy = 0;
	sfxinfo_t *info;
	int i;

	if(sound_id <= 0) return NULL;

	if(!freq) freq = &dummy;
	if(!volume) volume = &dummy;

	// Traverse all links when getting the definition.
	// (But only up to 10, which is certainly enough and prevents endless
	// recursion.) Update the sound id at the same time.
	// The links were checked in Def_Read() so there can't be any bogus ones.
	for(info = sounds + sound_id, i = 0;
		info->link && i < 10;
		info = info->link,
			*freq = (info->link_pitch > 0? info->link_pitch/128.0f : *freq),
			*volume += (info->link_volume != -1? info->link_volume/127.0f : 0),
			sound_id = info - sounds, i++);

	return info;
}

//===========================================================================
// S_IsRepeating
//	Returns true if the specified ID is a repeating sound.
//===========================================================================
boolean S_IsRepeating(int idFlags)
{
	sfxinfo_t *info;

	if(idFlags & DDSF_REPEAT) return true;

	info = S_GetSoundInfo(idFlags & ~DDSF_FLAG_MASK, NULL, NULL);
	return (info->flags & SF_REPEAT) != 0;
}

//===========================================================================
// S_LocalSoundAtVolumeFrom
//	Plays a sound on the local system. A public interface.
//	Origin and fixedpos can be both NULL, in which case the sound is
//	played in 2D and centered.
//	Flags can be included in the sound ID number (DDSF_*).
//	Returns nonzero if a sound was started.
//===========================================================================
int S_LocalSoundAtVolumeFrom
	(int soundIdAndFlags, mobj_t *origin, float *fixedPos, float volume)
{
	int soundId = soundIdAndFlags & ~DDSF_FLAG_MASK;
	sfxsample_t *sample;
	sfxinfo_t *info;
	float freq = 1;
	int result;
	boolean isRepeating = false;

	// A dedicated server never starts any local sounds
	// (only logical sounds in the LSM).
	if(isDedicated) return false;

	if(soundId <= 0
		|| soundId >= defs.count.sounds.num
		|| sfx_volume <= 0
		|| volume <= 0) return false; // This won't play...

#if _DEBUG
	if(volume > 1)
	{
		Con_Message("S_LocalSoundAtVolumeFrom: Warning! "
			"Too high volume (%f).\n", volume);
	}
#endif

	// This is the sound we're going to play.
	if((info = S_GetSoundInfo(soundId, &freq, &volume)) == NULL)
	{
		return false; // Hmm? This ID is not defined.
	}

	isRepeating = S_IsRepeating(soundIdAndFlags);

	// Check the distance (if applicable).
	if(!(info->flags & SF_NO_ATTENUATION)
		&& !(soundIdAndFlags & DDSF_NO_ATTENUATION))
	{
		// If origin is too far, don't even think about playing the sound.
		if(P_MobjPointDistancef(S_GetListenerMobj(), origin, fixedPos)
			> sound_max_distance) return false;
	}

	// Load the sample.
	if((sample = Sfx_Cache(soundId)) == NULL)
	{
		if(sfx_avail)
		{
			VERBOSE( Con_Message("S_LocalSoundAtVolumeFrom: Sound %i "
				"caching failed.\n", soundId) );
		}
		return false;
	}

	// Random frequency alteration? (Multipliers chosen to match
	// original sound code.)
	if(!nopitch)
	{
		if(info->flags & SF_RANDOM_SHIFT)
			freq += (M_FRandom() - M_FRandom()) * (7.0f / 255);
		if(info->flags & SF_RANDOM_SHIFT2)
			freq += (M_FRandom() - M_FRandom()) * (15.0f / 255);
	}

	// If the sound has an exclusion group, either all or the same emitter's
	// iterations of this sound will stop.
	if(info->group)
	{
		Sfx_StopSoundGroup(info->group,
			info->flags & SF_GLOBAL_EXCLUDE? NULL : origin);
	}

	// Let's play it.
	result = Sfx_StartSound(sample, volume, freq, origin, fixedPos,
		  (info->flags & SF_NO_ATTENUATION || soundIdAndFlags & DDSF_NO_ATTENUATION? SF_NO_ATTENUATION : 0)
		| (isRepeating? SF_REPEAT : 0)
		| (info->flags & SF_DONT_STOP? SF_DONT_STOP : 0));

	return result;
}

//===========================================================================
// S_LocalSoundAtVolume
//	Plays a sound on the local system.
//	This is a public sound interface.
//	Returns nonzero if a sound was started.
//===========================================================================
int S_LocalSoundAtVolume(int sound_id, mobj_t *origin, float volume)
{
	return S_LocalSoundAtVolumeFrom(sound_id, origin, NULL, volume);
}

//===========================================================================
// S_LocalSound
//	This is a public sound interface.
//	Returns nonzero if a sound was started.
//===========================================================================
int S_LocalSound(int sound_id, mobj_t *origin)
{
	// Play local sound at max volume.
	return S_LocalSoundAtVolumeFrom(sound_id, origin, NULL, 1);
}

//===========================================================================
// S_LocalSoundFrom
//	This is a public sound interface.
//	Returns nonzero if a sound was started.
//===========================================================================
int S_LocalSoundFrom(int sound_id, float *fixedpos)
{
	return S_LocalSoundAtVolumeFrom(sound_id, NULL, fixedpos, 1);
}

//=========================================================================
// S_StartSound
//	Play a world sound. All players in the game will hear it.
//	Returns nonzero if a sound was started.
//=========================================================================
int S_StartSound(int sound_id, mobj_t *origin)
{
	// The sound is audible to everybody.
	Sv_Sound(sound_id, origin, SVSF_TO_ALL);
	Sfx_StartLogical(sound_id, origin, S_IsRepeating(sound_id));

	return S_LocalSound(sound_id, origin);
}

//=========================================================================
// S_StartSoundAtVolume
//	Play a world sound. All players in the game will hear it.
//	Returns nonzero if a sound was started.
//=========================================================================
int S_StartSoundAtVolume(int sound_id, mobj_t *origin, float volume)
{
	Sv_SoundAtVolume(sound_id, origin, volume, SVSF_TO_ALL);
	Sfx_StartLogical(sound_id, origin, S_IsRepeating(sound_id));

	// The sound is audible to everybody.
	return S_LocalSoundAtVolume(sound_id, origin, volume);
}

//=========================================================================
// S_ConsoleSound
//	Play a player sound. Only the specified player will hear it.
//	Returns nonzero if a sound was started (always).
//=========================================================================
int S_ConsoleSound(int sound_id, mobj_t *origin, int target_console)
{
	Sv_Sound(sound_id, origin, target_console);

	// If it's for us, we can hear it.
	if(target_console == consoleplayer)
	{
		S_LocalSound(sound_id, origin);
	}
	return true;
}

//===========================================================================
// S_StopSound
//	If sound_id == 0, then stops all sounds of the origin.
//	If origin == NULL, stops all sounds with the ID.
//	Otherwise both ID and origin must match.
//===========================================================================
void S_StopSound(int sound_id, mobj_t *emitter)
{
	// Sfx provides a routine for this.
	Sfx_StopSound(sound_id, emitter);

	// Notify the LSM.
	if(Sfx_StopLogical(sound_id, emitter))
	{
		// In netgames, the server is responsible for telling clients
		// when to stop sounds. The LSM will tell us if a sound was
		// stopped somewhere in the world.
		Sv_StopSound(sound_id, emitter);
	}
}

//===========================================================================
// S_IsPlaying
//	Returns true if an instance of the sound is playing with the given
//	emitter. If sound_id is zero, returns true if the source is emitting
//	any sounds. An exported function.
//===========================================================================
int S_IsPlaying(int sound_id, mobj_t *emitter)
{
	// The Logical Sound Manager (under Sfx) provides a routine for this.
	return Sfx_IsPlaying(sound_id, emitter);
}

//===========================================================================
// S_StartMusicNum
//	Start a song based on its number. Returns true if the ID exists.
//===========================================================================
int S_StartMusicNum(int id, boolean looped)
{
	ded_music_t *def = defs.music + id;

	if(id < 0 || id >= defs.count.music.num) return false;
	// Don't play music if the volume is at zero.
	if(isDedicated) return true;
	if(verbose) Con_Message("S_StartMusic: %s.\n", def->id);
	return Mus_Start(def, looped);
}

//===========================================================================
// S_StartMusic
//	Returns true if the song is found.
//===========================================================================
int S_StartMusic(char *musicid, boolean looped)
{
	int idx = Def_GetMusicNum(musicid);

	if(idx < 0)
	{
		Con_Message("S_StartMusic: song %s not defined.\n", musicid);
		return false;
	}
	return S_StartMusicNum(idx, looped);
}

//===========================================================================
// S_StopMusic
//	Stops playing a song.
//===========================================================================
void S_StopMusic(void)
{
	Mus_Stop();
}

//===========================================================================
// S_Drawer
//	Draws debug information on-screen.
//===========================================================================
void S_Drawer(void)
{
	if(!sound_info) return;

	// Go into screen projection mode.
	gl.MatrixMode(DGL_PROJECTION);
	gl.PushMatrix();
	gl.LoadIdentity();
	gl.Ortho(0, 0, screenWidth, screenHeight, -1, 1);

	Sfx_DebugInfo();

	// Back to the original.
	gl.MatrixMode(DGL_PROJECTION);
	gl.PopMatrix();
}

//===========================================================================
// CCmdPlaySound
//	Console command for playing a sound effect.
//===========================================================================
int CCmdPlaySound(int argc, char **argv)
{
	int id = 0;
	float volume = 1;
	float fixedPos[3];
	int p;
	boolean useFixedPos = false;

	if(argc < 2)
	{
		Con_Printf("Usage: %s (id) (volume) at (x) (y) (z)\n", argv[0]);
		Con_Printf("(volume) must be in 0..1, but may be omitted.\n");
		Con_Printf("'at (x) (y) (z)' may also be omitted.\n");
		Con_Printf("The sound is always played locally.\n");
		return true;
	}

	// The sound ID is always first.
	id = Def_GetSoundNum(argv[1]);

	// The second argument may be a volume.
	if(argc >= 3 && stricmp(argv[2], "at"))
	{
		volume = strtod(argv[2], NULL);
		p = 3;
	}
	else
	{
		p = 2;
	}
	if(argc >= p + 4 && !stricmp(argv[p], "at"))
	{
        useFixedPos = true;
		fixedPos[VX] = strtod(argv[p + 1], NULL);
		fixedPos[VY] = strtod(argv[p + 2], NULL);
		fixedPos[VZ] = strtod(argv[p + 3], NULL);
	}

	// Check that the volume is valid.
	if(volume <= 0) return true;
	if(volume > 1) volume = 1;

	if(useFixedPos)
		S_LocalSoundAtVolumeFrom(id, NULL, fixedPos, volume);
	else
		S_LocalSoundAtVolume(id, NULL, volume);

	return true;
}
