//===========================================================================
// S_SFX.H
//===========================================================================
#ifndef __DOOMSDAY_SOUND_SFX_H__
#define __DOOMSDAY_SOUND_SFX_H__

#include "sys_sfxd.h"
#include "de_play.h"

typedef enum 
{
	SFXD_DSOUND,
	SFXD_A3D,
	SFXD_OPENAL,
	SFXD_COMPATIBLE
} 
sfxdriver_e;

typedef struct sfxcache_s
{
	struct sfxcache_s *next, *prev;
	sfxsample_t	sample;
	int hits;
	int lastused;			// Tic the sample was last hit.
}
sfxcache_t;

// Channel flags.
#define SFXCF_NO_ORIGIN			0x1	// Sound is coming from a mystical emitter.
#define SFXCF_NO_ATTENUATION	0x2	// Sound is very, very loud.

typedef struct sfxchannel_s
{
	int		flags;
	sfxbuffer_t *buffer;
	mobj_t	*emitter;		// Mobj that is emitting the sound.
	float	pos[3];			// Emit from here (synced with emitter).
	float	volume;			// Sound volume: 1.0 is max.
	float	frequency;		// Frequency adjustment: 1.0 is normal.
	int		starttime;		// When was the channel last started?
}
sfxchannel_t;

extern float	sfx_reverb_strength;
extern int		sfx_max_cache_kb, sfx_max_cache_tics; 
extern int		sound_3dmode, sound_16bit, sound_rate;

boolean		Sfx_Init(void);
void		Sfx_Shutdown(void);
void		Sfx_Reset(void);
void		Sfx_LevelChange(void);
void		Sfx_StartFrame(void);
void		Sfx_EndFrame(void);
void		Sfx_PurgeCache(void);
void		Sfx_RefreshChannels(void);
void		Sfx_StartSound(sfxsample_t *sample, float volume, float freq, mobj_t *emitter, float *fixedpos, int flags);
void		Sfx_StopSound(int id, mobj_t *emitter);
void		Sfx_StopSoundGroup(int group, mobj_t *emitter);
int			Sfx_IsPlaying(int id, mobj_t *emitter);
void		Sfx_DebugInfo(void);

#endif 