/**
 * @file fmod_sfx.h
 * Sound effects interface. @ingroup dsfmod
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

#ifndef __DSFMOD_SFX_H__
#define __DSFMOD_SFX_H__

#include "sys_audiod_sfx.h"

#ifdef __cplusplus
extern "C" {
#endif

int             DS_SFX_Init(void);
sfxbuffer_t*    DS_SFX_CreateBuffer(int flags, int bits, int rate);
void            DS_SFX_DestroyBuffer(sfxbuffer_t* buf);
void            DS_SFX_Load(sfxbuffer_t* buf, struct sfxsample_s* sample);
void            DS_SFX_Reset(sfxbuffer_t* buf);
void            DS_SFX_Play(sfxbuffer_t* buf);
void            DS_SFX_Stop(sfxbuffer_t* buf);
void            DS_SFX_Refresh(sfxbuffer_t* buf);
void            DS_SFX_Set(sfxbuffer_t* buf, int prop, float value);
void            DS_SFX_Setv(sfxbuffer_t* buf, int prop, float* values);
void            DS_SFX_Listener(int prop, float value);
void            DS_SFX_Listenerv(int prop, float* values);
int             DS_SFX_Getv(int prop, void* values);

#ifdef __cplusplus
}
#endif

#endif /* end of include guard: __DSFMOD_SFX_H__ */
