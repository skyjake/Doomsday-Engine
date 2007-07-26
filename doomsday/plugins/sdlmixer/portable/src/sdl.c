/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

/*
 * sdl.c: SDL_mixer implementation for Ext and Mus interfaces
 */

// HEADER FILES ------------------------------------------------------------

#include "driver.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int DM_Ext_PlayFile(const char *filename, int looped);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean sdlInitOk;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static unsigned songSize = 0;
static char *song;
static Mix_Music *currentMusic;

// CODE --------------------------------------------------------------------

void ExtMus_Shutdown(void)
{
	if(song)
		free(song);
	if(currentMusic)
		Mix_FreeMusic(currentMusic);

	song = NULL;
	currentMusic = NULL;
}

int DM_Ext_Init(void)
{
	// The music interface is available without any extra work.
	return sdlInitOk;
}

void DM_Ext_Update(void)
{
	// Nothing to update.
}

void DM_Ext_Set(int property, float value)
{
	if(!sdlInitOk)
		return;

	switch (property)
	{
	case MUSIP_VOLUME:
		Mix_VolumeMusic(MIX_MAX_VOLUME * value);
		break;
	}
}

int DM_Ext_Get(int property, void *value)
{
	if(!sdlInitOk)
		return false;

	switch (property)
	{
	case MUSIP_ID:
		strcpy(value, "SDLMixer/Ext");
		break;

	default:
		return false;
	}
	return true;
}

void *DM_Ext_SongBuffer(int length)
{
	if(!sdlInitOk)
		return NULL;

	if(song)
		free(song);
	songSize = length;
	return song = malloc(length);
}

int DM_Ext_PlayBuffer(int looped)
{
	if(!sdlInitOk)
		return false;

	if(song)
	{
		// Dump the song into a temporary file where SDL_mixer can
		// load it.
		FILE   *tmp = fopen(BUFFERED_MUSIC_FILE, "wb");

		if(tmp)
		{
			fwrite(song, songSize, 1, tmp);
			fclose(tmp);
		}

		free(song);
		song = 0;
		songSize = 0;
	}

	return DM_Ext_PlayFile(BUFFERED_MUSIC_FILE, looped);
}

void DM_Ext_Pause(int pause)
{
	if(!sdlInitOk)
		return;

	if(pause)
		Mix_PauseMusic();
	else
		Mix_ResumeMusic();
}

void DM_Ext_Stop(void)
{
	if(!sdlInitOk)
		return;

	Mix_HaltMusic();
}

static int playFile(const char *filename, int looped)
{
	if(!sdlInitOk)
		return false;

	// Free any previously loaded music.
	if(currentMusic)
		Mix_FreeMusic(currentMusic);

	if(!(currentMusic = Mix_LoadMUS(filename)))
	{
		DS_Error();
		return false;
	}

	return !Mix_PlayMusic(currentMusic, looped ? -1 : 1);
}

int DM_Ext_PlayFile(const char *filename, int looped)
{
	Mix_SetMusicCMD(NULL);
	return playFile(filename, looped);
}

int DM_Mus_Init(void)
{
	// No extra init needed.
	return sdlInitOk;
}

void DM_Mus_Update(void)
{
	// Nothing to update.
}

void DM_Mus_Set(int property, float value)
{
	// No MUS-specific properties exist.
}

int DM_Mus_Get(int property, void *value)
{
	if(!sdlInitOk)
		return false;

	switch (property)
	{
	case MUSIP_ID:
		strcpy(value, "SDLMixer/Mus");
		break;

	default:
		return false;
	}
	return true;
}

void DM_Mus_Pause(int pause)
{
	// Not needed.
}

void DM_Mus_Stop(void)
{
	// Not needed.
}

void *DM_Mus_SongBuffer(int length)
{
	return DM_Ext_SongBuffer(length);
}

int	DM_Mus_Play(int looped)
{
	char *command = getenv("DENG_MIDI_CMD");

	if(command == NULL)
		command = DEFAULT_MIDI_COMMAND;

	// If the midi command is empty, use NULL instead.
	if(command[0] == 0)
		command = NULL;
	
	convertMusToMidi(song, songSize, BUFFERED_MUSIC_FILE);
	Mix_SetMusicCMD(command);
	return playFile(BUFFERED_MUSIC_FILE, looped);
}
