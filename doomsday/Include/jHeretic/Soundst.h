
// soundst.h

#ifndef __SOUNDSTH__
#define __SOUNDSTH__

/*#define MAX_SND_DIST 	1600
#define MAX_CHANNELS	16

typedef struct
{
	mobj_t *mo;
	long sound_id;
	long handle;
	long pitch;
	int priority;
	int volume;
	char veryloud;
} channel_t;

typedef struct
{
	long id;
	unsigned short priority;
	char *name;
	mobj_t *mo;
	int distance;
} ChanInfo_t;

typedef	struct
{
	int channelCount;
	int musicVolume;
	int soundVolume;
	ChanInfo_t chan[16];
} SoundInfo_t;
*/
#define snd_MaxVolume	Get(DD_SFX_VOLUME)
#define snd_MusicVolume	Get(DD_MIDI_VOLUME)

//#include "s_common.h"
#include "R_Local.h"

void S_SectorSound(sector_t *sec, int id);

/*void S_Start(void);
void S_StartSound(mobj_t *origin, int sound_id);
void S_StartSoundAtVolume(mobj_t *origin, int sound_id, int volume);
void S_StopSound(mobj_t *origin);
void S_PauseSound(void);
void S_ResumeSound(void);
void S_UpdateSounds(mobj_t *listener);
void S_StartSong(int song, boolean loop);
void S_StopSong(void);
void S_Init(void);
void S_GetChannelInfo(SoundInfo_t *s);
void S_SetMaxVolume(boolean fullprocess);
void S_SetMusicVolume(void);
*/
/*void S_NetStartSound(mobj_t *origin, int sound_id);
void S_NetStartPlrSound(player_t *player, int sound_id);
void S_NetAvoidStartSound(mobj_t *origin, int sound_id, player_t *not_to_player);
void S_StartSectorSound(sector_t *sector, int sound_id);*/

#endif
