/* DE1: $Id$
 * Copyright (C) 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * driver.h: Common stuff for the SDL_mixer driver
 */

#ifndef __DOOMSDAY_SDLMIXER_AUDIO_H__
#define __DOOMSDAY_SDLMIXER_AUDIO_H__

#include "doomsday.h"
#include "sys_sfxd.h"
#include "sys_musd.h"
#include "mus2midi.h"

#include <stdlib.h>
#include <string.h>
#ifdef MACOSX
#  include <SDL/SDL.h>
#  include <SDL_mixer/SDL_mixer.h>
#else
#  include <SDL.h>
#  include <SDL_mixer.h>
#endif

#define BUFFERED_MUSIC_FILE "deng-sdlmixer-buffered-song"
#define DEFAULT_MIDI_COMMAND "" //"timidity"

enum { VX, VY, VZ };

void DS_Error(void);

// Shutdown the Ext and Mus interfaces.
void ExtMus_Shutdown(void);

#endif
