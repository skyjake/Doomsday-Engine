/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Ker�en <jaakko.keranen@iki.fi>
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
 * s_main.h: Sound Subsystem
 */

#ifndef __DOOMSDAY_SOUND_MAIN_H__
#define __DOOMSDAY_SOUND_MAIN_H__

#include "con_decl.h"
#include "p_mobj.h"
#include "def_main.h"

#define SF_RANDOM_SHIFT		0x1		// Random frequency shift.
#define SF_RANDOM_SHIFT2	0x2		// 2x bigger random frequency shift.
#define SF_GLOBAL_EXCLUDE	0x4		// Exclude all emitters.
#define SF_NO_ATTENUATION	0x8		// Very, very loud...
#define SF_REPEAT			0x10	// Repeats until stopped.
#define SF_DONT_STOP		0x20	// Sound can't be stopped while playing.

extern int sound_info;
extern int sound_min_distance, sound_max_distance;
extern int sfx_volume, mus_volume;

boolean		S_Init(void);
void		S_Shutdown(void);
void		S_LevelChange(void);
void		S_Reset(void);
void		S_StartFrame(void);
void		S_EndFrame(void);
sfxinfo_t *	S_GetSoundInfo(int sound_id, float *freq, float *volume);
mobj_t *	S_GetListenerMobj(void);
int			S_LocalSoundAtVolumeFrom(int sound_id, mobj_t *origin, float *fixedpos, float volume);
int			S_LocalSoundAtVolume(int sound_id, mobj_t *origin, float volume);
int			S_LocalSound(int sound_id, mobj_t *origin);
int			S_LocalSoundFrom(int sound_id, float *fixedpos);
int			S_StartSound(int sound_id, mobj_t *origin);
int			S_StartSoundAtVolume(int sound_id, mobj_t *origin, float volume);
int			S_ConsoleSound(int sound_id, mobj_t *origin, int target_console);
void		S_StopSound(int sound_id, mobj_t *origin);
int			S_IsPlaying(int sound_id, mobj_t *emitter);
int			S_StartMusic(char *musicid, boolean looped);
void		S_StopMusic(void);
void		S_Drawer(void);

D_CMD( PlaySound );

#endif
