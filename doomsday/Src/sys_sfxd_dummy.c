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
 * sys_sfxd_dummy.c: Dummy Music Driver
 *
 * Dummy Sfx Driver: Used in dedicated server mode, when it's necessary
 * to simulate sound playing but not actually play anything.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"
#include "sys_sfxd.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int				DS_DummyInit(void);
void			DS_DummyShutdown(void);
sfxbuffer_t*	DS_DummyCreateBuffer(int flags, int bits, int rate);
void			DS_DummyDestroyBuffer(sfxbuffer_t *buf);
void			DS_DummyLoad(sfxbuffer_t *buf, struct sfxsample_s *sample);
void			DS_DummyReset(sfxbuffer_t *buf);
void			DS_DummyPlay(sfxbuffer_t *buf);
void			DS_DummyStop(sfxbuffer_t *buf);
void			DS_DummyRefresh(sfxbuffer_t *buf);
void			DS_DummyEvent(int type);
void			DS_DummySet(sfxbuffer_t *buf, int property, float value);
void			DS_DummySetv(sfxbuffer_t *buf, int property, float *values);
void			DS_DummyListener(int property, float value);
void			DS_DummyListenerv(int property, float *values);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

sfxdriver_t	sfxd_dummy =
{
	DS_DummyInit,
	DS_DummyShutdown,
	DS_DummyCreateBuffer,
	DS_DummyDestroyBuffer,
	DS_DummyLoad,
	DS_DummyReset,
	DS_DummyPlay,
	DS_DummyStop,
	DS_DummyRefresh,
	DS_DummyEvent,
	DS_DummySet,
	DS_DummySetv,
	DS_DummyListener,
	DS_DummyListenerv
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean inited;

// CODE --------------------------------------------------------------------

//===========================================================================
// DS_DummyInit
//	Init DirectSound, start playing the primary buffer. Returns true
//	if successful.
//===========================================================================
int DS_DummyInit(void)
{
	if(inited) 
	{
		// Already initialized.
		return true; 
	}
	inited = true;
	return true;
}

//===========================================================================
// DS_DummyShutdown
//	Shut everything down.
//===========================================================================
void DS_DummyShutdown(void)
{
	inited = false;
}

//===========================================================================
// DS_DummyEvent
//	The Event function is called to tell the driver about certain critical
//	events like the beginning and end of an update cycle.
//===========================================================================
void DS_DummyEvent(int type)
{
	// Do nothing...
}

//===========================================================================
// DS_DummyCreateBuffer
//===========================================================================
sfxbuffer_t* DS_DummyCreateBuffer(int flags, int bits, int rate)
{
	sfxbuffer_t *buf;

	// Clear the buffer.
	buf = Z_Malloc(sizeof(*buf), PU_STATIC, 0);
	memset(buf, 0, sizeof(*buf));
	buf->bytes = bits/8;
	buf->rate = rate;
	buf->flags = flags;
	buf->freq = rate;		// Modified by calls to Set(SFXBP_FREQUENCY).
	return buf;
}

//===========================================================================
// DS_DummyDestroyBuffer
//===========================================================================
void DS_DummyDestroyBuffer(sfxbuffer_t *buf)
{
	// Free the memory allocated for the buffer.
	Z_Free(buf);
}

//===========================================================================
// DS_DummyLoad
//	Prepare the buffer for playing a sample by filling the buffer with 
//	as much sample data as fits. The pointer to sample is saved, so
//	the caller mustn't free it while the sample is loaded.
//===========================================================================
void DS_DummyLoad(sfxbuffer_t *buf, struct sfxsample_s *sample)
{
	// Now the buffer is ready for playing.
	buf->sample = sample;	
	buf->written = sample->size;
	buf->flags &= ~SFXBF_RELOAD;
}

//===========================================================================
// DS_DummyReset
//	Stops the buffer and makes it forget about its sample.
//===========================================================================
void DS_DummyReset(sfxbuffer_t *buf)
{
	DS_DummyStop(buf);
	buf->sample = NULL;
	buf->flags &= ~SFXBF_RELOAD;
}

//===========================================================================
// DS_DummyBufferLength
//	Returns the length of the buffer in milliseconds.
//===========================================================================
unsigned int DS_DummyBufferLength(sfxbuffer_t *buf)
{
	return 1000 * buf->sample->numsamples / buf->freq;
}

//===========================================================================
// DS_DummyPlay
//===========================================================================
void DS_DummyPlay(sfxbuffer_t *buf)
{
	// Playing is quite impossible without a sample.
	if(!buf->sample) return; 

	// Do we need to reload?
	if(buf->flags & SFXBF_RELOAD) DS_DummyLoad(buf, buf->sample);

	// The sound starts playing now?
	if(!(buf->flags & SFXBF_PLAYING))
	{
		// Calculate the end time (milliseconds).
		buf->endtime = Sys_GetRealTime() + DS_DummyBufferLength(buf);
	}

	// The buffer is now playing.
	buf->flags |= SFXBF_PLAYING;
}

//===========================================================================
// DS_DummyStop
//===========================================================================
void DS_DummyStop(sfxbuffer_t *buf)
{
	// Clear the flag that tells the Sfx module about playing buffers.
	buf->flags &= ~SFXBF_PLAYING;
	
	// If the sound is started again, it needs to be reloaded.
	buf->flags |= SFXBF_RELOAD;
}

//===========================================================================
// DS_DummyRefresh
//	Buffer streamer. Called by the Sfx refresh thread. 
//===========================================================================
void DS_DummyRefresh(sfxbuffer_t *buf)
{
	// Can only be done if there is a sample and the buffer is playing.
	if(!buf->sample || !(buf->flags & SFXBF_PLAYING)) return;

	// Have we passed the predicted end of sample?
	if(!(buf->flags & SFXBF_REPEAT) && Sys_GetRealTime() >= buf->endtime)
	{
		// Time for the sound to stop.
		DS_DummyStop(buf);
	}
}

//===========================================================================
// DS_DummySet
//	SFXBP_VOLUME (if negative, interpreted as attenuation)
//	SFXBP_FREQUENCY
//	SFXBP_PAN (-1..1)
//	SFXBP_MIN_DISTANCE
//	SFXBP_MAX_DISTANCE
//	SFXBP_RELATIVE_MODE
//===========================================================================
void DS_DummySet(sfxbuffer_t *buf, int property, float value)
{
	switch(property)
	{
	case SFXBP_FREQUENCY:
		buf->freq = buf->rate * value;
		break;
	}
}

//===========================================================================
// DS_DummySetv
//	SFXBP_POSITION
//	SFXBP_VELOCITY
//	Coordinates specified in world coordinate system, converted to DSound's: 
//	+X to the right, +Y up and +Z away (Y and Z swapped, i.e.).
//===========================================================================
void DS_DummySetv(sfxbuffer_t *buf, int property, float *values)
{
}

//===========================================================================
// DS_DummyListener
//	SFXLP_UNITS_PER_METER
//	SFXLP_DOPPLER
//	SFXLP_UPDATE
//===========================================================================
void DS_DummyListener(int property, float value)
{
}

//===========================================================================
// DS_DummyListenerEnvironment
//	Values use SRD_* for indices.
//===========================================================================
void DS_DummyListenerEnvironment(float *rev)
{
}

//===========================================================================
// DS_DummyListenerv
//	Call SFXLP_UPDATE at the end of every channel update.
//===========================================================================
void DS_DummyListenerv(int property, float *values)
{
}

