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
 * sys_musd.h: Music Driver
 */

#ifndef __DOOMSDAY_MUSIC_DRIVER_H__
#define __DOOMSDAY_MUSIC_DRIVER_H__

typedef struct musdriver_s
{
	int			(*Init)(void);
	void		(*Shutdown)(void);
} 
musdriver_t;

// Music interface properties.
enum
{
	MUSIP_ID,			// Only for Get()ing.
	MUSIP_VOLUME
};

// Generic driver interface. All other interfaces are based on this.
typedef struct musinterface_generic_s
{
	int			(*Init)(void);
	void		(*Update)(void);
	void		(*Set)(int property, float value);
	int			(*Get)(int property, void *value);
	void		(*Pause)(int pause);
	void		(*Stop)(void);
}
musinterface_generic_t;

// Driver interface for playing MUS music.
typedef struct musinterface_mus_s
{
	musinterface_generic_t gen;
	void *		(*SongBuffer)(int length);
	int			(*Play)(int looped);
}
musinterface_mus_t;

// Driver interface for playing non-MUS music.
typedef struct musinterface_ext_s
{
	musinterface_generic_t gen;
	void *		(*SongBuffer)(int length);
	int			(*PlayFile)(const char *filename, int looped);
	int			(*PlayBuffer)(int looped);
}
musinterface_ext_t;

// Driver interface for playing CD tracks.
typedef struct musinterface_cd_s
{
	musinterface_generic_t gen;
	int			(*Play)(int track, int looped);
}
musinterface_cd_t;

#endif 
