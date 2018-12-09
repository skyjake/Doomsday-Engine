/**
 * @file fmod_sfx.h
 * Sound effects interface. @ingroup dsfmod
 *
 * @authors Copyright © 2011-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html (with exception granted to allow
 * linking against FMOD Ex)
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses
 *
 * <b>Special Exception to GPLv2:</b>
 * Linking the Doomsday Audio Plugin for FMOD Ex (audio_fmod) statically or
 * dynamically with other modules is making a combined work based on
 * audio_fmod. Thus, the terms and conditions of the GNU General Public License
 * cover the whole combination. In addition, <i>as a special exception</i>, the
 * copyright holders of audio_fmod give you permission to combine audio_fmod
 * with free software programs or libraries that are released under the GNU
 * LGPL and with code included in the standard release of "FMOD Ex Programmer's
 * API" under the "FMOD Ex Programmer's API" license (or modified versions of
 * such code, with unchanged license). You may copy and distribute such a
 * system following the terms of the GNU GPL for audio_fmod and the licenses of
 * the other code concerned, provided that you include the source code of that
 * other code when and as the GNU GPL requires distribution of source code.
 * (Note that people who make modified versions of audio_fmod are not obligated
 * to grant this special exception for their modified versions; it is their
 * choice whether to do so. The GNU General Public License gives permission to
 * release a modified version without this exception; this exception also makes
 * it possible to release a modified version which carries forward this
 * exception.) </small>
 */

#ifndef __DSFMOD_SFX_H__
#define __DSFMOD_SFX_H__

#include "api_audiod_sfx.h"

int             fmod_DS_SFX_Init(void);
sfxbuffer_t*    fmod_DS_SFX_CreateBuffer(int flags, int bits, int rate);
void            fmod_DS_SFX_DestroyBuffer(sfxbuffer_t* buf);
void            fmod_DS_SFX_Load(sfxbuffer_t* buf, struct sfxsample_s* sample);
void            fmod_DS_SFX_Reset(sfxbuffer_t* buf);
void            fmod_DS_SFX_Play(sfxbuffer_t* buf);
void            fmod_DS_SFX_Stop(sfxbuffer_t* buf);
void            fmod_DS_SFX_Refresh(sfxbuffer_t* buf);
void            fmod_DS_SFX_Set(sfxbuffer_t* buf, int prop, float value);
void            fmod_DS_SFX_Setv(sfxbuffer_t* buf, int prop, float* values);
void            fmod_DS_SFX_Listener(int prop, float value);
void            fmod_DS_SFX_Listenerv(int prop, float* values);
int             fmod_DS_SFX_Getv(int prop, void* values);

#endif /* end of include guard: __DSFMOD_SFX_H__ */
