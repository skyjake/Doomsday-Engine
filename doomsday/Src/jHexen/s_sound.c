
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
//#include "s_common.h"

// MACROS ------------------------------------------------------------------

#define DEFAULT_ARCHIVEPATH     "o:\\sound\\archive\\"
#define PRIORITY_MAX_ADJUST 10
#define DIST_ADJUST (MAX_SND_DIST/PRIORITY_MAX_ADJUST)*/

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void S_StopSoundID(int sound_id);//, int priority);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

#if 0
int			s_CDTrack = 0;
boolean		MusicPaused;
int			snd_MaxChannels = 20;
#endif

char		ArchivePath[128];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------


#if 0
/*
===============================================================================

		MUSIC & SFX API

===============================================================================
*/

channel_t			*Channel = 0;
int					numChannels = 0;

int					RegisteredSong = 0; //the current registered song.

int					Mus_Song = -1;
int					Mus_LumpNum;
void				*Mus_SndPtr;
byte				*SoundCurve;

extern	int			startepisode;
extern	int			startmap;


//==========================================================================
//
// S_Start
//
//==========================================================================

void S_Start(void)
{
	S_StopAllSound();
	S_StartSong(gamemap, true);
}

//==========================================================================
//
// S_StartSong
//
//==========================================================================

void S_StartSong(int song, boolean loop)
{
	char *songLump;
	int track;

	if(i_CDMusic)
	{ // Play a CD track, instead
		if(s_CDTrack)
		{ // Default to the player-chosen track
			track = s_CDTrack;
		}
		else
		{
			track = P_GetMapCDTrack(gamemap);
		}
		if(track == gi.CD(DD_GET_CURRENT_TRACK,0) && gi.CD(DD_GET_TIME_LEFT,0) > 0
			&& gi.CD(DD_STATUS,0) == DD_PLAYING)
		{
			// The chosen track is already playing.
			return;
		}
		if(gi.CD(loop? DD_PLAY_LOOP : DD_PLAY, track))
			Con_Printf( "Error starting CD play (track %d).\n", track);
	}
	else
	{
		if(song == Mus_Song)
		{ // don't replay an old song
			return;
		}
		if(RegisteredSong)
		{
			gi.StopSong();
			/*if(UseSndScript)
			{
				Z_Free(Mus_SndPtr);
			}
			else
			{*/
				gi.W_ChangeCacheTag(Mus_LumpNum, PU_CACHE);
			//}
			RegisteredSong = 0;
		}
		songLump = P_GetMapSongLump(song);
		if(!songLump)
		{
			return;
		}
		/*if(UseSndScript)
		{
			char name[128];
			sprintf(name, "%s%s.lmp", ArchivePath, songLump);
			M_ReadFile(name, (byte **)&Mus_SndPtr);
		}
		else
		{*/
			Mus_LumpNum = W_GetNumForName(songLump);
			Mus_SndPtr = W_CacheLumpNum(Mus_LumpNum, PU_MUSIC);
		//}
		RegisteredSong = gi.PlaySong(Mus_SndPtr, W_LumpLength(Mus_LumpNum), loop);
		Mus_Song = song;
	}
}

//===========================================================================
// S_StopSong
//===========================================================================
void S_StopSong(void)
{
	gi.StopSong();
	if(Mus_LumpNum >= 0) gi.W_ChangeCacheTag(Mus_LumpNum, PU_CACHE);	
	Mus_LumpNum = -1;
	Mus_Song = 0;
	RegisteredSong = 0;
}

//==========================================================================
//
// S_StartSongName
//
//==========================================================================

void S_StartSongName(char *songLump, boolean loop)
{
	int cdTrack;

	if(!songLump)
	{
		return;
	}
	if(i_CDMusic)
	{
		cdTrack = 0;

		if(!strcmp(songLump, "hexen"))
		{
			cdTrack = P_GetCDTitleTrack();
		}
		else if(!strcmp(songLump, "hub"))
		{
			cdTrack = P_GetCDIntermissionTrack();
		}
		else if(!strcmp(songLump, "hall"))
		{
			cdTrack = P_GetCDEnd1Track();
		}
		else if(!strcmp(songLump, "orb"))
		{
			cdTrack = P_GetCDEnd2Track();
		}
		else if(!strcmp(songLump, "chess") && !s_CDTrack)
		{
			cdTrack = P_GetCDEnd3Track();
		}
/*	Uncomment this, if Kevin writes a specific song for startup
		else if(!strcmp(songLump, "start"))
		{
			cdTrack = P_GetCDStartTrack();
		}
*/
		if(!cdTrack || (cdTrack == gi.CD(DD_GET_CURRENT_TRACK,0) && gi.CD(DD_GET_TIME_LEFT,0) > 0))
		{
			return;
		}
		if(!gi.CD(loop? DD_PLAY_LOOP : DD_PLAY, cdTrack))
		{
			// Clear the user selection?
			s_CDTrack = false;
		}
	}
	else
	{
		if(RegisteredSong)
		{
			gi.StopSong();
			/*if(UseSndScript)
			{
				Z_Free(Mus_SndPtr);
			}
			else
			{*/
				gi.W_ChangeCacheTag(Mus_LumpNum, PU_CACHE);
			//}
			RegisteredSong = 0;
		}
		/*if(UseSndScript)
		{
			char name[128];
			sprintf(name, "%s%s.lmp", ArchivePath, songLump);
			gi.ReadFile(name, (byte **)&Mus_SndPtr);
		}
		else
		{*/
			Mus_LumpNum = W_GetNumForName(songLump);
			Mus_SndPtr = W_CacheLumpNum(Mus_LumpNum, PU_MUSIC);
		//}
		RegisteredSong = gi.PlaySong(Mus_SndPtr, 
			W_LumpLength(Mus_LumpNum), loop);
		Mus_Song = -1;
	}
}
#endif

//==========================================================================
// S_GetSoundID
//==========================================================================
int S_GetSoundID(char *name)
{
	return Def_Get(DD_DEF_SOUND_BY_NAME, name, 0);
}


#if 0
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

//==========================================================================
//
// S_StopSoundID
//
//==========================================================================

void S_StopSoundID(int sound_id)
{
	int		i;

	for(i=0; i<numChannels; i++)
		if(Channel[i].sound_id == sound_id && Channel[i].mo)
		{
			S_StopChannel(Channel + i);
			break;
		}
}

//==========================================================================
//
// S_StopSound
//
//==========================================================================

void S_StopSound(mobj_t *origin)
{
	int i;

	for(i=0; i<numChannels; i++)
	{
		if(Channel[i].mo == origin)
		{
			S_StopChannel(Channel + i);
		}
	}
}

//==========================================================================
//
// S_StopAllSound
//
//==========================================================================

void S_StopAllSound(void)
{
	int i;

	// Stop all sounds.
	for(i=0; i<numChannels; i++) 
		S_StopChannel(Channel + i);
	memset(Channel, 0, numChannels * sizeof(channel_t));
	listenerSector = NULL;
}

//==========================================================================
//
// S_SoundLink
//
//==========================================================================

void S_SoundLink(mobj_t *oldactor, mobj_t *newactor)
{
	int i;

	for(i=0; i<numChannels; i++)
		if(Channel[i].mo == oldactor)
			Channel[i].mo = newactor;
}

//==========================================================================
//
// S_PauseSound
//
//==========================================================================

void S_PauseSound(void)
{
	S_StopAllSound();
	if(i_CDMusic)
	{
		gi.CD(DD_STOP,0);
	}
	else
	{
		gi.PauseSong();
	}
}

//==========================================================================
//
// S_ResumeSound
//
//==========================================================================

void S_ResumeSound(void)
{
	if(i_CDMusic)
	{
		gi.CD(DD_RESUME, 0);
	}
	else
	{
		gi.ResumeSong();
	}
}


//==========================================================================
//
// S_Init
//
//==========================================================================

void S_Init(void)
{
	SoundCurve = W_CacheLumpName("SNDCURVE", PU_STATIC);
	numChannels = 0;
	Channel = NULL;
}

//==========================================================================
//
// S_GetChannelInfo
//
//==========================================================================

void S_GetChannelInfo(SoundInfo_t *s)
{
	int i;
	ChanInfo_t *c;

	s->channelCount = numChannels<16? numChannels : 16;
	s->musicVolume = 0;
	s->soundVolume = 0;

	for(i = 0; i < s->channelCount; i++)
	{
		c = &s->chan[i];
		c->id = Channel[i].sound_id;
		c->priority = Channel[i].priority;
		c->name = S_sfx[c->id].lumpname;
		c->mo = Channel[i].mo;
		if(c->mo)
			c->distance = P_ApproxDistance(c->mo->x-Get(DD_VIEWX), 
			c->mo->y-Get(DD_VIEWY))>>FRACBITS;
		else
			c->distance = -1;
	}
}

//==========================================================================
//
// S_GetSoundPlayingInfo
//
//==========================================================================

boolean S_GetSoundPlayingInfo(mobj_t *mobj, int sound_id)
{
	int i;

	for(i=0; i<numChannels; i++)
		if(Channel[i].sound_id == sound_id && Channel[i].mo == mobj)
			if(gi.SoundIsPlaying(Channel[i].handle))
				return true;
	return false;
}

//==========================================================================
//
// S_ShutDown
//
//==========================================================================

void S_ShutDown(void)
{
	free(Channel);
	Channel = 0;
	numChannels = 0;
}

void S_Reset(void)
{
	int		i;

	S_StopAllSound();
	Z_FreeTags(PU_SOUND, PU_SOUND);
	for(i=0; i<NUMSFX; i++)
	{
		S_sfx[i].lumpnum = 0;
		S_sfx[i].data = NULL;				
	}
	// Music, too.
	if(RegisteredSong)
	{
		gi.StopSong();
		/*if(UseSndScript)
			Z_Free(Mus_SndPtr);
		else*/
			gi.W_ChangeCacheTag(Mus_LumpNum, PU_CACHE);
		RegisteredSong = 0;
	}
	Mus_Song = -1;
}
#endif

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
				strcpy(ArchivePath, sc_String);
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



