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
 * s_mus.h: Music Subsystem
 */

#ifndef __DOOMSDAY_SOUND_MUSIC_H__
#define __DOOMSDAY_SOUND_MUSIC_H__

#include "con_decl.h"
#include "sys_musd.h"

// Music preference. If multiple resources are available, this setting
// is used to determine which one to use (mus < ext < cd).
enum
{
	MUSP_MUS,
	MUSP_EXT,
	MUSP_CD
};

extern int	mus_preference;

boolean		Mus_Init(void);
void		Mus_Shutdown(void);
void		Mus_SetVolume(float vol);
void		Mus_Pause(boolean do_pause);
void		Mus_StartFrame(void);
int			Mus_Start(ded_music_t *def, boolean looped);
void		Mus_Stop(void);

// Console commands.
D_CMD( PlayMusic );
D_CMD( PlayExt );
D_CMD( StopMusic );

#endif 
