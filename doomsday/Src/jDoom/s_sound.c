
//**************************************************************************
//**
//** S_SOUND.C
//**
//** Based on Heretic sound code.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "DoomDef.h"
#include "doomstat.h"
#include "R_local.h"
#include "P_local.h"
#include "d_config.h"
#include "s_sound.h"
#include "s_common.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

channel_t	*Channel = 0;
int			numChannels = 0;
byte		*SoundCurve;

int			RegisteredSong;
int			MusicPaused;
int			Mus_Song = -1;
int			Mus_LumpNum = -1;
void		*Mus_SndPtr;
int			s_CDTrack = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void S_StopChannel(channel_t *chan)
{
	if(chan->handle)
	{
		gi.StopSound(chan->handle);
		if(S_sfx[chan->sound_id].usefulness > 0)
			S_sfx[chan->sound_id].usefulness--;
		memset(chan, 0, sizeof(*chan));
	}
}

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
			mus_e3m4,	// American	e4m1
			mus_e3m2,	// Romero	e4m2
			mus_e3m3,	// Shawn	e4m3
			mus_e1m5,	// American	e4m4
			mus_e2m7,	// Tim 		e4m5
			mus_e2m4,	// Romero	e4m6
			mus_e2m6,	// J.Anderson	e4m7 CHIRON.WAD
			mus_e2m5,	// Shawn	e4m8
			mus_e1m9	// Tim		e4m9
		};
		
		if (episode < 4)
			mnum = mus_e1m1 + (episode-1)*9 + map-1;
		else
			mnum = spmus[map-1];
	}	
	return mnum;
}

void S_LevelMusic(void)
{
	ddmapinfo_t info;
	char id[10];

	if(gamestate != GS_LEVEL || IS_DEDICATED) return;

	Mus_Song = -1;

	sprintf(id, "E%iM%i", gameepisode, gamemap);
	if(Def_Get(DD_DEF_MAP_INFO, id, &info)
		&& info.music >= 0)
	{
		// Use the music in the Map Info definition.
		S_ChangeMusic(info.music, true);
	}
	else
	{
		// Start the default song for the map.
		S_ChangeMusic(S_GetMusicNum(gameepisode, gamemap), true);
	}
}

void S_Start(void)
{
	int i;
	
	// Stop all sounds.
	for(i=0; i < numChannels; i++)
	{
		if(Channel[i].handle)
			S_StopChannel(Channel+i);
	}
	// Reset all channels.
	memset(Channel, 0, numChannels*sizeof(channel_t));
	listenerSector = NULL;
}

//
// Starts some music with the music id found in sounds.h.
//
void S_StartMusic(int m_id)
{
    S_ChangeMusic(m_id, false);
}

void S_ChangeMusic(int song, boolean loop)
{
	if(song == Mus_Song)
	{ // don't replay an old song
		return;
	}
	// Stop currently playing song.
	gi.StopSong();
	if(Mus_LumpNum >= 0) W_ChangeCacheTag(Mus_LumpNum, PU_CACHE);		
	if(song < 1 || song > MAXMUSIC) return;
	// External song?
	if(S_music[song].extfile[0] && cfg.customMusic)
	{
		Mus_LumpNum = -1;
		Mus_SndPtr = 0;
		gi.PlaySong(S_music[song].extfile, DDMUSICF_EXTERNAL, loop);
	}
	else
	{
		Mus_LumpNum = W_GetNumForName(S_music[song].lumpname);
		Mus_SndPtr = W_CacheLumpNum(Mus_LumpNum, PU_MUSIC);
		gi.PlaySong(Mus_SndPtr, W_LumpLength(Mus_LumpNum), loop);
	}
	Mus_Song = song;
}

void S_StopMusic(void)
{
	if(Mus_LumpNum < 0) return;
	
	gi.StopSong();
	if(Mus_SndPtr) Z_ChangeTag2(Mus_SndPtr, PU_CACHE);
	Mus_LumpNum = -1;
	Mus_SndPtr = 0;
}

void S_StopSound(mobj_t *origin)
{
	int i;

	for(i=0; i<numChannels; i++)
	{
		if(Channel[i].mo == origin)
			S_StopChannel(&Channel[i]);
	}
}

// Stop only the 'sfxnum' sounds that originate from 'origin'.
void S_StopSoundNum(mobj_t *origin, int sfxnum)
{
	int i;

	for(i=0; i<numChannels; i++)
	{
		if(Channel[i].mo == origin && Channel[i].sound_id == sfxnum)
			S_StopChannel(&Channel[i]);
	}
}

void S_SoundLink(mobj_t *oldactor, mobj_t *newactor)
{
	int i;

	for(i=0;i<numChannels;i++)
	{
		if(Channel[i].mo == oldactor)
			Channel[i].mo = newactor;
	}
}

void S_PauseSound(void)
{
	gi.PauseSong();
}

void S_ResumeSound(void)
{
	gi.ResumeSong();
}

void S_Init(void)
{
	SoundCurve = Z_Malloc(MAX_SND_DIST, PU_STATIC, NULL);
	memcpy(SoundCurve, W_CacheLumpName("SNDCURVE", PU_CACHE), 
		MAX_SND_DIST);
}

void S_ShutDown(void)
{
}

#if 0
void S_GetChannelInfo(SoundInfo_t *s)
{
	int i;
	ChanInfo_t *c;

	s->channelCount = numChannels > 16? 16 : numChannels;
	s->musicVolume = snd_MusicVolume;
	s->soundVolume = snd_MaxVolume;
	for(i = 0; i < s->channelCount; i++)
	{
		c = &s->chan[i];
		c->id = Channel[i].sound_id;
		c->priority = S_sfx[c->id].usefulness; 
		c->name = S_sfx[c->id].name;
		c->mo = Channel[i].mo;
		if(c->mo)
			c->distance = Channel[i].volume;
		else
			c->distance = 0;
	}
}
#endif
