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
 * sys_sfxd.h: Sound Driver
 *
 * System-specific Sfx Driver.
 */

#ifndef __DOOMSDAY_SFX_DRIVER_H__
#define __DOOMSDAY_SFX_DRIVER_H__

// Sfx Buffer Flags.
#define SFXBF_PLAYING	0x1		// The buffer is playing.
#define SFXBF_3D		0x2		// Otherwise playing in 2D mode.
#define SFXBF_REPEAT	0x4		// Buffer will repeat until stopped.
#define SFXBF_DONT_STOP	0x8		// Never stop until normal finish.
#define SFXBF_RELOAD	0x10000	// Sample must be reloaded before playing.

// Events.
enum
{
	SFXEV_BEGIN,				// An update is about to begin.
	SFXEV_END					// The update is done.
};

// Buffer properties.
enum
{
	SFXBP_VOLUME,				// 0..1
	SFXBP_FREQUENCY,			// 1 = normal
	SFXBP_PAN,					// -1..1 (2D only)
	SFXBP_MIN_DISTANCE,			// 3D only
	SFXBP_MAX_DISTANCE,			
	SFXBP_POSITION,				
	SFXBP_VELOCITY,
	SFXBP_RELATIVE_MODE
};

// Listener properties.
enum
{
	SFXLP_UPDATE,				// Not a real value (commit deferred)
	SFXLP_PRIMARY_FORMAT,		// Arguments are bits and rate.
	SFXLP_UNITS_PER_METER,
	SFXLP_DOPPLER,
	SFXLP_POSITION,
	SFXLP_VELOCITY,
	SFXLP_ORIENTATION,
	SFXLP_REVERB				// Use SRD_* for indices.
};

typedef struct sfxsample_s
{
	int		id;				// Id number of the sound sample.
	void	*data;			// Actual sample data.
	unsigned int size;		// Size in bytes.
	int		numsamples;		// Number of samples.
	int		bytesper;		// Bytes per sample (1 or 2).
	int		rate;			// Samples per second.
	int		group;			// Exclusion group (0, if none).
}
sfxsample_t;

typedef struct sfxbuffer_s
{
	void *ptr;					// Pointer to driver's own buffer object.
	void *ptr3d;				// Pointer to driver's 3D buffer data.
	struct sfxsample_s *sample;	// Source sample data.
	int bytes;					// Bytes per sample (1 or 2).
	int rate;					// Samples per second.
	int flags;
	unsigned int length;		// Length of the buffer (bytes).
	unsigned int cursor;		// Write cursor position (%length).
	unsigned int written;		// Total bytes written.
	unsigned int endtime;		// System time, milliseconds (if !repeating).
	unsigned int freq;			// Played samples per second (real freq).
}
sfxbuffer_t;

typedef struct sfxdriver_s
{
	int			(*Init)(void);
	void		(*Shutdown)(void);
	sfxbuffer_t*(*Create)(int flags, int bits, int rate);
	void		(*Destroy)(sfxbuffer_t *buf);
	void		(*Load)(sfxbuffer_t *buf, struct sfxsample_s *sample);
	void		(*Reset)(sfxbuffer_t *buf);
	void		(*Play)(sfxbuffer_t *buf);
	void		(*Stop)(sfxbuffer_t *buf);
	void		(*Refresh)(sfxbuffer_t *buf);
	void		(*Event)(int type);
	void		(*Set)(sfxbuffer_t *buf, int property, float value);
	void		(*Setv)(sfxbuffer_t *buf, int property, float *values);
	void		(*Listener)(int property, float value);
	void		(*Listenerv)(int property, float *values);
	int			(*Getv)(int property, void *values);
}
sfxdriver_t;

#endif 
