//===========================================================================
// S_MAIN.H
//===========================================================================
#ifndef __DOOMSDAY_SOUND_MAIN_H__
#define __DOOMSDAY_SOUND_MAIN_H__

#define SF_RANDOM_SHIFT		0x1		// Random frequency shift.
#define SF_RANDOM_SHIFT2	0x2		// 2x bigger random frequency shift.
#define SF_GLOBAL_EXCLUDE	0x4		// Exclude all emitters.
#define SF_NO_ATTENUATION	0x8		// Very, very loud...
#define SF_REPEAT			0x10	// Repeats until stopped.

extern int sound_info;
extern int sound_min_distance, sound_max_distance;
extern int sfx_volume, mus_volume;

boolean		S_Init(void);
void		S_Shutdown(void);
void		S_LevelChange(void);
void		S_Reset(void);
void		S_StartFrame(void);
void		S_EndFrame(void);
mobj_t *	S_GetListenerMobj(void);
void		S_LocalSoundAtVolumeFrom(int sound_id, mobj_t *origin, float *fixedpos, float volume);
void		S_LocalSoundAtVolume(int sound_id, mobj_t *origin, float volume);
void		S_LocalSound(int sound_id, mobj_t *origin);
void		S_LocalSoundFrom(int sound_id, float *fixedpos);
void		S_StartSound(int sound_id, mobj_t *origin);
void		S_StartSoundAtVolume(int sound_id, mobj_t *origin, float volume);
void		S_ConsoleSound(int sound_id, mobj_t *origin, int target_console);
void		S_StopSound(int sound_id, mobj_t *origin);
int			S_IsPlaying(int sound_id, mobj_t *origin);
int			S_StartMusic(char *musicid, boolean looped);
void		S_StopMusic(void);
void		S_Drawer(void);

#endif 