#include "DoomDef.h"
#include "R_local.h"
#include "P_local.h"
#include "i_sound.h"
#include "soundst.h"
#include "settings.h"
#include "s_common.h"


boolean S_StopSoundID(int sound_id, int priority);

/*
===============================================================================

		MUSIC & SFX API

===============================================================================
*/

channel_t	*Channel = 0;
int			numChannels = 0;


int RegisteredSong;
int MusicPaused;
int Mus_Song = -1;
int Mus_LumpNum = -1;
void *mus_sndptr;
byte *SoundCurve;
int s_CDTrack = 0;

extern int startepisode;
extern int startmap;

int AmbChan;

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

void S_LevelMusic(void)
{
	ddmapinfo_t info;
	char id[10];

	if(gamestate != GS_LEVEL) return;

	Mus_Song = -1;

	sprintf(id, "E%iM%i", gameepisode, gamemap);
	if(Def_Get(DD_DEF_MAP_INFO, id, &info)
		&& info.music >= 0)
	{
		S_StartSong(info.music, true);
	}
	else
	{
		S_StartSong((gameepisode-1)*9 + gamemap-1, true);
	}
}

void S_Start(void)
{
	int i;
	
	S_LevelMusic();

	//stop all sounds
	for(i=0; i < numChannels; i++)
	{
		if(Channel[i].handle)
		{
			S_StopChannel(Channel+i);
			if(AmbChan == i)
			{
				AmbChan = -1;
			}
		}
	}
	// Reset all channels.
	memset(Channel, 0, numChannels*sizeof(channel_t));
	listenerSector = NULL;
}

void S_StartSong(int song, boolean loop)
{
	if(song == Mus_Song)
	{ // don't replay an old song
		return;
	}
	gi.StopSong();
	if(Mus_LumpNum >= 0) W_ChangeCacheTag(Mus_LumpNum, PU_CACHE);		
	if(song < mus_e1m1 || song > MAXMUSIC)
	{
		return;
	}
	if(S_music[song].extfile[0] && cfg.customMusic)
	{
		Mus_LumpNum = -1;
		mus_sndptr = 0;
		gi.PlaySong(S_music[song].extfile, DDMUSICF_EXTERNAL, true);
	}
	else
	{
		Mus_LumpNum = W_GetNumForName(S_music[song].lumpname);
		mus_sndptr = W_CacheLumpNum(Mus_LumpNum, PU_MUSIC);
		gi.PlaySong(mus_sndptr, W_LumpLength(Mus_LumpNum), true);
	}
	Mus_Song = song;
}

void S_StopSong(void)
{
	gi.StopSong();
	if(Mus_LumpNum >= 0) W_ChangeCacheTag(Mus_LumpNum, PU_CACHE);	
	Mus_LumpNum = -1;
	Mus_Song = 0;
}

void S_StopSound(mobj_t *origin)
{
	int i;

	for(i=0;i<numChannels;i++)
	{
		if(Channel[i].mo == origin)
		{
			S_StopChannel(&Channel[i]);
			if(AmbChan == i)
			{
				AmbChan = -1;
			}
		}
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
	S_SetMaxVolume(true);
}

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

void S_SetMaxVolume(boolean fullprocess)
{
	int i;

	if(!fullprocess)
	{
		SoundCurve[0] = (*((byte *)W_CacheLumpName("SNDCURVE", PU_CACHE))
			* (snd_MaxVolume*8))>>7;
	}
	else
	{
		for(i = 0; i < MAX_SND_DIST; i++)
		{
			SoundCurve[i] = (*((byte *)W_CacheLumpName("SNDCURVE", PU_CACHE)+i)) * 255/127;
		}
	}
}

void S_ShutDown(void)
{
}


