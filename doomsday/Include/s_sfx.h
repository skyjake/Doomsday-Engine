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
 * s_sfx.h: Sound Effects
 */

#ifndef __DOOMSDAY_SOUND_SFX_H__
#define __DOOMSDAY_SOUND_SFX_H__

#include "sys_sfxd.h"
#include "de_play.h"

// Begin and end macros for Critical OPerations. They are operations 
// that can't be done while a refresh is being made. No refreshing 
// will be done between BEGIN_COP and END_COP.
#define BEGIN_COP		Sfx_AllowRefresh(false)
#define END_COP			Sfx_AllowRefresh(true)

typedef enum 
{
	SFXD_DSOUND,
	SFXD_A3D,
	SFXD_OPENAL,
	SFXD_COMPATIBLE,
	SFXD_SDL_MIXER,
	SFXD_DUMMY
} 
sfxdriver_e;

// Channel flags.
#define SFXCF_NO_ORIGIN			0x1	// Sound is coming from a mystical emitter.
#define SFXCF_NO_ATTENUATION	0x2	// Sound is very, very loud.
#define SFXCF_NO_UPDATE			0x4	// Channel update is skipped.

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

extern boolean	sfx_avail;
extern float	sfx_reverb_strength;
extern int		sfx_max_cache_kb, sfx_max_cache_tics; 
extern int		sfx_bits, sfx_rate;
extern int		sound_3dmode, sound_16bit, sound_rate;

boolean		Sfx_Init(void);
void		Sfx_Shutdown(void);
void		Sfx_Reset(void);
void		Sfx_AllowRefresh(boolean allow);
void		Sfx_LevelChange(void);
void		Sfx_StartFrame(void);
void		Sfx_EndFrame(void);
void		Sfx_PurgeCache(void);
void		Sfx_RefreshChannels(void);
int			Sfx_StartSound(sfxsample_t *sample, float volume, float freq, mobj_t *emitter, float *fixedpos, int flags);
int			Sfx_StopSound(int id, mobj_t *emitter);
void		Sfx_StopSoundGroup(int group, mobj_t *emitter);
int			Sfx_CountPlaying(int id);
void		Sfx_UnloadSoundID(int id);
//int		Sfx_IsPlaying(int id, mobj_t *emitter);
void		Sfx_DebugInfo(void);

#endif 
