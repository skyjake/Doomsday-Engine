/**
 * @file fmod_music.h
 * Music playback interface. @ingroup dsfmod
 *
 * @authors Copyright © 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef __DSFMOD_MUS_H__
#define __DSFMOD_MUS_H__

#include <fmod.h>
#include <fmod.hpp>
#include "sys_audiod_mus.h"

#ifdef __cplusplus
extern "C" {
#endif

int     DM_Music_Init(void);
void    DM_Music_Shutdown(void);
void    DM_Music_Set(int prop, float value);
int     DM_Music_Get(int prop, void* ptr);
void    DM_Music_Update(void);
void*   DM_Music_SongBuffer(unsigned int length); // buffered play supported
int     DM_Music_Play(int looped);
void    DM_Music_Stop(void);
void    DM_Music_Pause(int setPause);
int     DM_Music_PlayFile(const char *filename, int looped);

#ifdef __cplusplus
}
#endif

// Internal:
void    DMFmod_Music_Shutdown(void);
bool    DMFmod_Music_PlaySound(FMOD::Sound* customSound, bool needRelease);
void    DMFmod_Music_Pause(int setPause);
void    DMFmod_Music_Stop(void);
void    DMFmod_Music_Set(int prop, float value);
int     DMFmod_Music_Get(int prop, void* ptr);
void    DMFmod_Music_SetSoundFont(const char* fileName);

#endif /* end of include guard: __DSFMOD_MUS_H__ */
