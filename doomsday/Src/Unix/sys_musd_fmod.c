
//**************************************************************************
//**
//** SYS_MUSD_FMod.C
//**
//** FMOD Mus Driver
//**
//** Plays CD music and non-MUS songs (MIDI, MP3, MOD...)
//** using the excellent FMOD. Link with fmodvc.lib.
//**
//**************************************************************************

#if 0

// HEADER FILES ------------------------------------------------------------

#include <fmod.h>
#include <fmod_errors.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_audio.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

int		DM_FModInit(void);
void	DM_FModShutdown(void);

		// Ext Interface.
int		DM_FModExtInit(void);
void	DM_FModExtShutdown(void);
void	DM_FModExtReset();
void	DM_FModExtUpdate(void);
void	DM_FModExtSet(int property, float value);
int		DM_FModExtGet(int property, void *ptr);
void	DM_FModExtPause(int pause);
void	DM_FModExtStop(void);
void *	DM_FModExtSongBuffer(int length);
int		DM_FModExtPlayFile(const char *path, int looped);
int		DM_FModExtPlayBuffer(int looped);

		// CD Interface.
int		DM_FModCDInit(void);
void	DM_FModCDShutdown(void);
void	DM_FModCDUpdate(void);
void	DM_FModCDSet(int property, float value);
int		DM_FModCDGet(int property, void *ptr);
void	DM_FModCDPause(int pause);
void	DM_FModCDStop(void);
int		DM_FModCDPlay(int track, int looped);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

musdriver_t musd_fmod = 
{
	DM_FModInit,
	DM_FModShutdown
};

musinterface_ext_t musd_fmod_iext =
{
	DM_FModExtInit,
	DM_FModExtUpdate,
	DM_FModExtSet,
	DM_FModExtGet,
	DM_FModExtPause,
	DM_FModExtStop,
	DM_FModExtSongBuffer,
	DM_FModExtPlayFile,
	DM_FModExtPlayBuffer
};

musinterface_cd_t musd_fmod_icd =
{
	DM_FModCDInit,
	DM_FModCDUpdate,
	DM_FModCDSet,
	DM_FModCDGet,
	DM_FModCDPause,
	DM_FModCDStop,
	DM_FModCDPlay
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean	inited = false, ext_inited = false;
static int		ext_volume = 200;
//static boolean	ext_looped_play, ext_playing;
//static int		ext_start_time;
static int		original_cd_volume;
static void		*song;
static int		song_size, stream_channel;
static FMUSIC_MODULE *module;
static FSOUND_STREAM *stream;

// CODE --------------------------------------------------------------------

//===========================================================================
// DM_FModInit
//===========================================================================
int DM_FModInit(void)
{
	if(inited) return true;

	if(FSOUND_GetVersion() < FMOD_VERSION)
	{
		Con_Message("DM_FModInit: You are using the wrong version of FMOD.DLL!\n"
			"  You should be using version %.02f.\n", FMOD_VERSION);
		return false;
	}
	if(!FSOUND_SetOutput(FSOUND_OUTPUT_DSOUND))
	{
		Con_Message("DM_FModInit: Can't use DirectSound.\n");
		if(!FSOUND_SetOutput(FSOUND_OUTPUT_WINMM))
		{
			Con_Message("DM_FModInit: Can't use WINMM!! Aborting...\n");
			return false;
		}
	}
	if(!FSOUND_Init(44100, 16, 0))
	{
		Con_Message("DM_FModInit: Init failed. (%s)\n", 
			FMOD_ErrorString(FSOUND_GetError()));
		return false;
	}

	ext_inited = false;
	return inited = true;
}

//===========================================================================
// DM_FModShutdown
//===========================================================================
void DM_FModShutdown(void)
{
	if(!inited) return;
	inited = false;

	// Shut down the interfaces.
	DM_FModExtShutdown();
	DM_FModCDShutdown();

	FSOUND_Close();
}

//===========================================================================
// DM_FModExtInit
//===========================================================================
int	DM_FModExtInit(void)
{
	if(ext_inited) return true;

	// Clear the song buffer.
	song = NULL;
	song_size = 0;
	module = NULL;
	stream = NULL;

	return ext_inited = true;
}

//===========================================================================
// DM_FModExtShutdown
//===========================================================================
void DM_FModExtShutdown(void)
{
	if(!ext_inited) return;
	ext_inited = false;

	DM_FModExtReset();

	// Free the song buffer.
	if(song) free(song);
	song = NULL;
	song_size = 0;
}

//===========================================================================
// DM_FModExtReset
//===========================================================================
void DM_FModExtReset(void)
{
	// Get rid of old module/stream.
	if(module) FMUSIC_FreeSong(module);
	if(stream) FSOUND_Stream_Close(stream);
	module = 0;
	stream = 0;
	stream_channel = -1;
}

//===========================================================================
// DM_FModExtUpdate
//===========================================================================
void DM_FModExtUpdate(void)
{
	if(!ext_inited) return;
	/*if(module
		&& ext_playing 
		&& ext_looped_play 
		&& Sys_GetTime() - ext_start_time > TICSPERSEC*3
		&& !FMUSIC_IsPlaying(module))
	{
		FMUSIC_PlaySong(module);
	}*/
}

//===========================================================================
// DM_FModExtSet
//===========================================================================
void DM_FModExtSet(int property, float value)
{
	if(!ext_inited) return;
	switch(property)
	{
	case MUSIP_VOLUME:
		// This affects streams.
		FSOUND_SetSFXMasterVolume(ext_volume = value*255 + 0.5);
		if(module) FMUSIC_SetMasterVolume(module, ext_volume);
		break;
	}
}

//===========================================================================
// DM_FModExtGet
//===========================================================================
int DM_FModExtGet(int property, void *ptr)
{
	if(!ext_inited) return false;
	switch(property)
	{
	case MUSIP_ID:
		strcpy(ptr, "FMod/Ext");
		break;
	
	default:
		return false;
	}
	return true;
}

//===========================================================================
// DM_FModExtPause
//===========================================================================
void DM_FModExtPause(int pause)
{
	if(!ext_inited) return;
	//ext_playing = !pause;
	if(module) FMUSIC_SetPaused(module, pause != 0);
	if(stream) FSOUND_SetPaused(stream_channel, pause != 0);
}

//===========================================================================
// DM_FModExtStop
//===========================================================================
void DM_FModExtStop(void)
{
	if(!ext_inited) return;
	//ext_playing = false;
	if(module) FMUSIC_StopSong(module);
	if(stream) FSOUND_Stream_Stop(stream);
}

//===========================================================================
// DM_FModExtSongBuffer
//===========================================================================
void* DM_FModExtSongBuffer(int length)
{
	if(!ext_inited) return NULL;
	// Free the previous data.
	if(song) free(song);
	song_size = length;
	return song = malloc(song_size);
}

//===========================================================================
// DM_FModExtStartPlaying
//===========================================================================
void DM_FModExtStartPlaying(void)
{
	if(module) 
	{
		FMUSIC_SetMasterVolume(module, ext_volume);
		FMUSIC_PlaySong(module);
	}
	if(stream) 
	{
		stream_channel = FSOUND_Stream_Play(FSOUND_FREE, stream);
	}
//	ext_playing = true;
//	ext_start_time = Sys_GetTime();
}

//===========================================================================
// DM_FModExtPlayFile
//===========================================================================
int	DM_FModExtPlayFile(const char *path, int looped)
{
	if(!ext_inited) return false;
	DM_FModExtReset();

	// Try playing as a module first.
	if((module = FMUSIC_LoadSong(path)))
	{
		FMUSIC_SetLooping(module, looped);		
	}
	else
	{
		// Try as a stream.
		stream = FSOUND_Stream_Open(path, 
			looped? FSOUND_LOOP_NORMAL : 0, 0, 0);
		if(!stream) return false; // Failed...!
	}
	DM_FModExtStartPlaying();
	//ext_looped_play = looped;
	return true;
}

//===========================================================================
// DM_FModExtPlayBuffer
//===========================================================================
int	DM_FModExtPlayBuffer(int looped)
{
	if(!ext_inited) return false;
	DM_FModExtReset();

	// Try playing as a module first.
	if((module = FMUSIC_LoadSongEx(song, 0, song_size, 
		FSOUND_LOADMEMORY, NULL, 0)))
	{
		FMUSIC_SetLooping(module, looped);
	}
	else
	{
		// Try as a stream.
		stream = FSOUND_Stream_Open(song,
			FSOUND_LOADMEMORY | (looped? FSOUND_LOOP_NORMAL : 0), 
			0, song_size);
		if(!stream) return false;
	}
	DM_FModExtStartPlaying();	
	//ext_looped_play = looped;
	return true;
}

//===========================================================================
// DM_FModCDInit
//===========================================================================
int	DM_FModCDInit(void)
{
	original_cd_volume = FSOUND_CD_GetVolume(0);
	// If FMOD is OK, then is this.
	return inited; 
}

//===========================================================================
// DM_FModCDShutdown
//===========================================================================
void DM_FModCDShutdown(void)
{
	// Stop playing the CD track.
	FSOUND_CD_Stop(0);
	FSOUND_CD_SetVolume(0, original_cd_volume);
}

//===========================================================================
// DM_FModCDUpdate
//===========================================================================
void DM_FModCDUpdate(void)
{
	// Nothing needs to be done.
}

//===========================================================================
// DM_FModCDSet
//===========================================================================
void DM_FModCDSet(int property, float value)
{
	if(!inited) return;

	switch(property)
	{
	case MUSIP_VOLUME:
		FSOUND_CD_SetVolume(0, value*255 + 0.5);
		break;
	}
}

//===========================================================================
// DM_FmodCDGet
//===========================================================================
int DM_FModCDGet(int property, void *ptr)
{
	if(!ext_inited) return false;
	switch(property)
	{
	case MUSIP_ID:
		strcpy(ptr, "FMod/CD");
		break;
	
	default:
		return false;
	}
	return true;
}

//===========================================================================
// DM_FModCDPause
//===========================================================================
void DM_FModCDPause(int pause)
{
	if(!inited) return;
	FSOUND_CD_SetPaused(0, pause != 0);
}

//===========================================================================
// DM_FModCDStop
//===========================================================================
void DM_FModCDStop(void)
{
	if(!inited) return;
	FSOUND_CD_Stop(0);
}

//===========================================================================
// DM_FModCDPlay
//===========================================================================
int	DM_FModCDPlay(int track, int looped)
{
	if(!inited) return false;
	FSOUND_CD_SetPlayMode(0, looped? FSOUND_CD_PLAYLOOPED 
		: FSOUND_CD_PLAYONCE);
	return FSOUND_CD_Play(0, track);
}

#endif
