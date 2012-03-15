/**
 * @file fmod_cd.h
 * CD audio playback. @ingroup dsfmod
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

#ifndef __FMOD_CD_H__
#define __FMOD_CD_H__

#include "sys_audiod_mus.h"

#ifdef __cplusplus
extern "C" {
#endif

int     DM_CDAudio_Init(void);
void    DM_CDAudio_Shutdown(void);
void    DM_CDAudio_Update(void);
void    DM_CDAudio_Set(int prop, float value);
int     DM_CDAudio_Get(int prop, void* value);
int     DM_CDAudio_Play(int track, int looped);
void    DM_CDAudio_Pause(int pause);
void    DM_CDAudio_Stop(void);

#ifdef __cplusplus
}
#endif

#endif /* end of include guard: __FMOD_CD_H__ */
