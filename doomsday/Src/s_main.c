
//**************************************************************************
//**
//** S_MAIN.C
//**
//** Interface to the Sfx and Mus modules. 
//** High-level (and exported) sound control.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"

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

	// There is no sound or music in dedicated mode.
	if(isDedicated || ArgExists("-nosound")) return true;

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
// S_LocalSoundAtVolumeFrom
//	Plays a sound on the local system. A public interface.
//	Origin and fixedpos can be both NULL, in which case the sound is
//	played in 2D and centered.
//	Flags can be included in the sound ID number (DDSF_*).
//===========================================================================
void S_LocalSoundAtVolumeFrom
	(int sound_id_and_flags, mobj_t *origin, float *fixedpos, float volume)
{
	int sound_id = sound_id_and_flags & ~DDSF_FLAG_MASK;
	sfxsample_t samp;
	sfxinfo_t *info;
	float freq = 1;
	int i;
	unsigned short *sp = NULL;
	boolean needfree = false;
	char buf[300];

	if(sound_id <= 0 
		|| sound_id >= defs.count.sounds.num
		|| sfx_volume <= 0
		|| volume <= 0) return; // This won't play...

#if _DEBUG
	if(volume > 1)
	{
		Con_Message("S_LocalSoundAtVolumeFrom: Warning! "
			"Too high volume (%f).\n", volume);
	}
#endif

	// Figure out where to get the sample data for this sound.
	// It might be from a data file such as a WAD or external sound 
	// resources. The definition and the configuration settings will
	// help us in making the decision.
	
	// Traverse all links when getting the definition.
	// (But only up to 10, which is certainly enough and prevents endless
	// recursion.) Update the sound id at the same time.
	// The links were checked in Def_Read() so there can't be any bogus ones.
	for(info = sounds + sound_id, i = 0; 
		info->link && i < 10; 
		info = info->link, 
			freq = (info->link_pitch > 0? info->link_pitch/128.0f : freq),
			volume += (info->link_volume != -1? info->link_volume/127.0f : 0),
			sound_id = info - sounds, i++);

	// This is the sound we're going to play.
	if(sound_id <= 0) return; // Hmm?

	// Check the distance (if applicable).
	if(!(info->flags & SF_NO_ATTENUATION)
		&& !(sound_id_and_flags & DDSF_NO_ATTENUATION))
	{
		// If origin is too far, don't even think about playing the sound.
		if(P_MobjPointDistancef(S_GetListenerMobj(), origin, fixedpos)
			> sound_max_distance) return;
	}

	samp.id = sound_id;	
	samp.group = info->group;
	samp.data = NULL;

	// Has an external sound file been defined?
	if(info->external[0])
	{
		// Yes, try loading. Like music, the file name is relative to the
		// base path.
		M_PrependBasePath(info->external, buf);
		if((samp.data = WAV_Load(buf, &samp.bytesper, &samp.rate, 
			&samp.numsamples)))
		{
			// Loading was successful!
			needfree = true;
			samp.bytesper /= 8; // Was returned as bits.
		}
	}

	// No sample loaded yet?
	if(!samp.data)
	{
		// Try loading from the lump.
		if(info->lumpnum < 0)
		{
			Con_Error("S_LocalSound: Sound %s has a bad lump: '%s'.\n",
				info->id, info->lumpname);
		}
		sp = W_CacheLumpNum(info->lumpnum, PU_STATIC);
		
		// Is this perhaps a WAV sound?
		if(WAV_CheckFormat((char*)sp))
		{
			// Load as WAV, then.
			if(!(samp.data = WAV_MemoryLoad((char*)sp, 
				W_LumpLength(info->lumpnum),
				&samp.bytesper, &samp.rate, &samp.numsamples)))
			{
				// Abort...
				Con_Message("S_LocalSound: WAV data in lump %s is bad.\n",
					info->lumpname);
				W_CacheLumpNum(info->lumpnum, PU_CACHE);
				return;
			}			
			needfree = true;
			samp.bytesper /= 8;
		}
		else
		{
			// Must be an old-fashioned DOOM sample.
			samp.data = sp + 4;		// Eight byte header.
			samp.bytesper = 1;		// 8-bit.
			samp.rate = sp[1];		// Sample rate.
			samp.numsamples = *(int*)(sp + 2);
		}
	}

	samp.size = samp.bytesper * samp.numsamples;

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

	// Sample acquired, let's play it.
	Sfx_StartSound(&samp, volume, freq, origin, fixedpos,
		(info->flags & SF_NO_ATTENUATION || sound_id_and_flags & DDSF_NO_ATTENUATION? SF_NO_ATTENUATION : 0)
		| (info->flags & SF_REPEAT || sound_id_and_flags & DDSF_REPEAT? SF_REPEAT : 0));

	// We don't need the original sample any more, clean up.
	if(sp) W_ChangeCacheTag(info->lumpnum, PU_CACHE);
	if(needfree) Z_Free(samp.data);
}

//===========================================================================
// S_LocalSoundAtVolume
//	Plays a sound on the local system.
//	This is a public sound interface.
//===========================================================================
void S_LocalSoundAtVolume(int sound_id, mobj_t *origin, float volume)
{
	S_LocalSoundAtVolumeFrom(sound_id, origin, NULL, volume);
}

//===========================================================================
// S_LocalSound
//	This is a public sound interface.
//===========================================================================
void S_LocalSound(int sound_id, mobj_t *origin)
{
	// Play local sound at max volume.
	S_LocalSoundAtVolumeFrom(sound_id, origin, NULL, 1);
}

//===========================================================================
// S_LocalSoundFrom
//	This is a public sound interface.
//===========================================================================
void S_LocalSoundFrom(int sound_id, float *fixedpos)
{
	S_LocalSoundAtVolumeFrom(sound_id, NULL, fixedpos, 1);
}

//=========================================================================
// S_StartSound
//	Play a world sound. All players in the game will hear it.
//=========================================================================
void S_StartSound(int sound_id, mobj_t *origin)
{
	// The sound is audible to everybody.
	Sv_Sound(sound_id, origin, SVSF_TO_ALL);
	S_LocalSound(sound_id, origin);
}

//=========================================================================
// S_StartSoundAtVolume
//	Play a world sound. All players in the game will hear it.
//=========================================================================
void S_StartSoundAtVolume(int sound_id, mobj_t *origin, float volume)
{
	// The sound is audible to everybody.
	Sv_SoundAtVolume(sound_id, origin, volume*127 + 0.5f, SVSF_TO_ALL);
	S_LocalSoundAtVolume(sound_id, origin, volume);
}

//=========================================================================
// S_ConsoleSound
//	Play a player sound. Only the specified player will hear it.
//=========================================================================
void S_ConsoleSound(int sound_id, mobj_t *origin, int target_console)
{
	Sv_Sound(sound_id, origin, target_console);

	// If it's for us, we can hear it.
	if(target_console == consoleplayer) 
		S_LocalSound(sound_id, origin);
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
}

//===========================================================================
// S_IsPlaying
//	Returns true if an instance of the sound is playing with the given
//	emitter. If sound_id is zero, returns true if the source is emitting
//	any sounds.
//===========================================================================
int S_IsPlaying(int sound_id, mobj_t *emitter)
{
	// Sfx provides a routine for this.
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
	if(isDedicated || !mus_volume) return true;
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
