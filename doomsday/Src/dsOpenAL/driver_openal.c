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
 * driver_openal.c: OpenAL Doomsday Sfx Driver
 *
 * Link with openal32.lib (and doomsday.lib).
 */

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#pragma warning (disable: 4244)
#endif

#include <AL/al.h>
#include <AL/alc.h>
#include <string.h>
#include <math.h>

#include "doomsday.h"
#include "sys_sfxd.h"

// MACROS ------------------------------------------------------------------

#define PI			3.141592654

#define Src(buf)	((ALuint)buf->ptr3d)
#define Buf(buf)	((ALuint)buf->ptr)

// TYPES -------------------------------------------------------------------

enum { VX, VY, VZ };

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

#ifdef WIN32
ALenum (*EAXGet)(const struct _GUID *propertySetID, ALuint property, ALuint source, ALvoid *value, ALuint size);
ALenum (*EAXSet)(const struct _GUID *propertySetID, ALuint property, ALuint source, ALvoid *value, ALuint size);
#endif

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int				DS_Init(void);
void			DS_Shutdown(void);
sfxbuffer_t*	DS_CreateBuffer(int flags, int bits, int rate);
void			DS_DestroyBuffer(sfxbuffer_t *buf);
void			DS_Load(sfxbuffer_t *buf, struct sfxsample_s *sample);
void			DS_Reset(sfxbuffer_t *buf);
void			DS_Play(sfxbuffer_t *buf);
void			DS_Stop(sfxbuffer_t *buf);
void			DS_Refresh(sfxbuffer_t *buf);
void			DS_Event(int type);
void			DS_Set(sfxbuffer_t *buf, int property, float value);
void			DS_Setv(sfxbuffer_t *buf, int property, float *values);
void			DS_Listener(int property, float value);
void			DS_Listenerv(int property, float *values);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

#ifdef WIN32
// EAX 2.0 GUIDs
struct _GUID DSPROPSETID_EAX20_ListenerProperties = { 0x306a6a8, 0xb224, 0x11d2, { 0x99, 0xe5, 0x0, 0x0, 0xe8, 0xd8, 0xc7, 0x22 } };
struct _GUID DSPROPSETID_EAX20_BufferProperties = { 0x306a6a7, 0xb224, 0x11d2, {0x99, 0xe5, 0x0, 0x0, 0xe8, 0xd8, 0xc7, 0x22 } };
#endif

boolean				initOk = false, hasEAX = false;
int					verbose;
float				unitsPerMeter = 1;
float				headYaw, headPitch; // In radians.
ALCdevice			*device = 0;
ALCcontext			*context = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// Error
//===========================================================================
int Error(const char *where, const char *msg)
{
	ALenum code = alGetError();
	if(code == AL_NO_ERROR) return false;
	Con_Message("DS_%s(OpenAL): %s [%s]\n", where, msg, alGetString(code));
	return true;
}

//===========================================================================
// DS_Init
//===========================================================================
int DS_Init(void)
{
	if(initOk) return true;

	// Are we in verbose mode?	
	if((verbose = ArgExists("-verbose")))
		Con_Message("DS_Init(OpenAL): Starting OpenAL...\n");

	// Open device.
	if(!(device = alcOpenDevice((ALubyte*)"DirectSound3D")))
	{
		Con_Message("Failed to initialize OpenAL (DS3D).\n");
		return false;		
	}
	// Create a context.
	alcMakeContextCurrent(context = alcCreateContext(device, NULL));

	// Clear error message.
	alGetError();

#ifdef WIN32
	// Check for EAX 2.0.
	if((hasEAX = alIsExtensionPresent((ALubyte*)"EAX2.0")))
	{
		if(!(EAXGet = alGetProcAddress("EAXGet"))) hasEAX = false;
		if(!(EAXSet = alGetProcAddress("EAXSet"))) hasEAX = false;
	}
	if(hasEAX && verbose)
		Con_Message("DS_Init(OpenAL): EAX 2.0 available.\n");
#else
	hasEAX = false;
#endif

	alListenerf(AL_GAIN, 1);
	alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
	headYaw = headPitch = 0;
	unitsPerMeter = 36;

	// Everything is OK.
	initOk = true;
	return true;
}

//===========================================================================
// DS_Shutdown
//===========================================================================
void DS_Shutdown(void)
{
	if(!initOk) return;

	alcMakeContextCurrent(NULL);
	alcDestroyContext(context);
	alcCloseDevice(device);

	context = NULL;
	device = NULL;	
	initOk = false;
}

//===========================================================================
// DS_CreateBuffer
//===========================================================================
sfxbuffer_t* DS_CreateBuffer(int flags, int bits, int rate)
{
	sfxbuffer_t *buf;
	ALuint bufferName, sourceName;

/*	IA3dSource2 *src;
	sfxbuffer_t *buf;
	bool play3d = (flags & SFXBF_3D) != 0;
	
	// Create a new source.
	if(FAILED(hr = a3d->NewSource(play3d? A3DSOURCE_TYPEDEFAULT 
		: A3DSOURCE_INITIAL_RENDERMODE_NATIVE, &src))) return NULL;

	// Set its format.
	WAVEFORMATEX format, *f = &format;
	memset(f, 0, sizeof(*f));
	f->wFormatTag = WAVE_FORMAT_PCM;
	f->nChannels = 1;
	f->nSamplesPerSec = rate;
	f->nBlockAlign = bits/8;
	f->nAvgBytesPerSec = f->nSamplesPerSec * f->nBlockAlign;
	f->wBitsPerSample = bits;
	src->SetAudioFormat((void*)f);
*/

	// Create a new buffer and a new source.
	alGenBuffers(1, &bufferName);
	if(Error("CreateBuffer", "GenBuffers")) return 0;
	
	alGenSources(1, &sourceName);
	if(Error("CreateBuffer", "GenSources")) 
	{
		alDeleteBuffers(1, &bufferName);
		return 0;
	}

	// Attach the buffer to the source.
	alSourcei(sourceName, AL_BUFFER, bufferName);
	Error("CreateBuffer", "Source BUFFER");

	if(!(flags & SFXBF_3D)) 
	{
		// 2D sounds are around the listener.
		alSourcei(sourceName, AL_SOURCE_RELATIVE, AL_TRUE);
		alSourcef(sourceName, AL_ROLLOFF_FACTOR, 0);
	}
	
	// Create the buffer object.
	buf = (sfxbuffer_t*) Z_Malloc(sizeof(*buf), PU_STATIC, 0);
	memset(buf, 0, sizeof(*buf));
	buf->ptr = (void*) bufferName;
	buf->ptr3d = (void*) sourceName;
	buf->bytes = bits/8;
	buf->rate = rate;
	buf->flags = flags;
	buf->freq = rate;		// Modified by calls to Set(SFXBP_FREQUENCY).
	return buf;
}

//===========================================================================
// DS_DestroyBuffer
//===========================================================================
void DS_DestroyBuffer(sfxbuffer_t *buf)
{
	ALuint srcName = Src(buf);
	ALuint bufName = Buf(buf);

	alDeleteSources(1, &srcName);
	alDeleteBuffers(1, &bufName);

	Z_Free(buf);
}

//===========================================================================
// DS_Load
//===========================================================================
void DS_Load(sfxbuffer_t *buf, struct sfxsample_s *sample)
{
	// Does the buffer already have a sample loaded?
	if(buf->sample)
	{	
		// Is the same one?
		if(buf->sample->id == sample->id) return; // No need to reload.
	}

	alBufferData(Buf(buf), 
		sample->bytesper == 1? AL_FORMAT_MONO8 : AL_FORMAT_MONO16,
		sample->data, sample->size, sample->rate);

/*	alGetBufferi(Buf(buf), AL_SIZE, &d);
	Con_Message("Loaded = %i\n", d);
	alGetBufferi(Buf(buf), AL_FREQUENCY, &d);
	Con_Message("Freq = %i\n", d);
	alGetBufferi(Buf(buf), AL_BITS, &d);
	Con_Message("Bits = %i\n", d);*/
		
	Error("Load", "BufferData");
	buf->sample = sample;		
}

//===========================================================================
// DS_Reset
//	Stops the buffer and makes it forget about its sample.
//===========================================================================
void DS_Reset(sfxbuffer_t *buf)
{
	DS_Stop(buf);
	buf->sample = NULL;
	// Unallocate the resources of the source.
	//Src(buf)->FreeAudioData();
}

//===========================================================================
// DS_Play
//===========================================================================
void DS_Play(sfxbuffer_t *buf)
{
	ALuint source = Src(buf), bn;
	ALint i;
	float f;

	// Playing is quite impossible without a sample.
	if(!buf->sample) return;
	
/*	alGetSourcei(source, AL_BUFFER, &bn);
	Con_Message("Buffer = %x\n", bn);
	if(bn != Buf(buf)) Con_Message("Not the same!\n");*/
	alSourcei(source, AL_BUFFER, Buf(buf));
	alSourcei(source, AL_LOOPING, (buf->flags & SFXBF_REPEAT) != 0);
	alSourcePlay(source);
	Error("Play", "SourcePlay");

	alGetSourcei(source, AL_BUFFER, &bn);
	Con_Message("Buffer = %x (real = %x), isBuf:%i\n", bn, Buf(buf),
		alIsBuffer(bn));

	alGetBufferi(bn, AL_SIZE, &i);
	Con_Message("Bufsize = %i bytes\n", i);

	alGetBufferi(bn, AL_BITS, &i);
	Con_Message("Bufbits = %i\n", i);

	alGetSourcef(source, AL_GAIN, &f);
	Con_Message("Gain = %g\n", f);

	alGetSourcef(source, AL_PITCH, &f);
	Con_Message("Pitch = %g\n", f);

	alGetSourcei(source, AL_SOURCE_STATE, &i);
	Error("Play", "Get state");
	Con_Message("State = %x\n", i);
	if(i != AL_PLAYING) Con_Message("not playing...\n");

	// The buffer is now playing.
	buf->flags |= SFXBF_PLAYING;
}

//===========================================================================
// DS_Stop
//===========================================================================
void DS_Stop(sfxbuffer_t *buf)
{
	if(!buf->sample) return;
	alSourceRewind(Src(buf));
	buf->flags &= ~SFXBF_PLAYING;
}

//===========================================================================
// DS_Refresh
//===========================================================================
void DS_Refresh(sfxbuffer_t *buf)
{
	ALint state;

	if(!buf->sample) return;
	
	alGetSourcei(Src(buf), AL_SOURCE_STATE, &state);
	if(state == AL_STOPPED)
	{
		buf->flags &= ~SFXBF_PLAYING;
	}
}

//===========================================================================
// DS_Event
//===========================================================================
void DS_Event(int type)
{
	/*switch(type)
	{
	case SFXEV_BEGIN:
		a3d->Clear();
		break;

	case SFXEV_END:
		a3d->Flush();
		break;
	}*/
}

//===========================================================================
// Vectors
//	Specify yaw and pitch in radians. 'up' can be NULL.
//===========================================================================
void Vectors(float yaw, float pitch, float *front, float *up)
{
	front[VX] = (float) (cos(yaw) * cos(pitch));
	front[VZ] = (float) (sin(yaw) * cos(pitch));
	front[VY] = (float) sin(pitch);

	if(up)
	{
		up[VX] = (float) (-cos(yaw) * sin(pitch));
		up[VZ] = (float) (-sin(yaw) * sin(pitch));
		up[VY] = (float) cos(pitch);
	}
}

//===========================================================================
// SetPan
//	Pan is linear, from -1 to 1. 0 is in the middle.
//===========================================================================
void SetPan(ALuint source, float pan)
{
/*	float gains[2];
	
	if(pan < -1) pan = -1;
	if(pan > 1) pan = 1;

	if(pan == 0)
	{
		gains[0] = gains[1] = 1;	// In the center.
	}
	else if(pan < 0) // On the left?
	{
		gains[0] = 1;
		//gains[1] = pow(.5, (100*(1 - 2*pan))/6);
		gains[1] = 1 + pan;
	}
	else if(pan > 0) // On the right?
	{
		//gains[0] = pow(.5, (100*(2*(pan-0.5)))/6);
		gains[0] = 1 - pan;
		gains[1] = 1;
	}
	source->SetPanValues(2, gains);*/
	
	float pos[3];

	//alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
	Vectors(headYaw - pan*PI/2, headPitch, pos, 0);
	alSourcefv(source, AL_POSITION, pos);
}

//===========================================================================
// DS_Set
//===========================================================================
void DS_Set(sfxbuffer_t *buf, int property, float value)
{
	unsigned int dw;
/*	IA3dSource2 *s = Src(buf);
	float minDist, maxDist;*/
	ALuint source = Src(buf);

	switch(property)
	{
	case SFXBP_VOLUME:
		alSourcef(source, AL_GAIN, value);
		break;

	case SFXBP_FREQUENCY:
		dw = (int) (buf->rate * value);
		if(dw != buf->freq)	// Don't set redundantly.
		{
			buf->freq = dw;
			alSourcef(source, AL_PITCH, value);
		}
		break;

	case SFXBP_PAN:
		SetPan(source, value);
		break;

	case SFXBP_MIN_DISTANCE:
		alSourcef(source, AL_REFERENCE_DISTANCE, value/unitsPerMeter);
		break;

	case SFXBP_MAX_DISTANCE:
		alSourcef(source, AL_MAX_DISTANCE, value/unitsPerMeter);
		break;

	case SFXBP_RELATIVE_MODE:
		alSourcei(source, AL_SOURCE_RELATIVE, value? AL_TRUE : AL_FALSE);
		break;
	}
}

//===========================================================================
// DS_Setv
//===========================================================================
void DS_Setv(sfxbuffer_t *buf, int property, float *values)
{
	ALuint source = Src(buf);

	switch(property)
	{
	case SFXBP_POSITION:
		alSource3f(source, AL_POSITION,
			values[VX]/unitsPerMeter,
			values[VZ]/unitsPerMeter,
			values[VY]/unitsPerMeter);
		break;

	case SFXBP_VELOCITY:
		alSource3f(source, AL_VELOCITY,
			values[VX]/unitsPerMeter,
			values[VZ]/unitsPerMeter,
			values[VY]/unitsPerMeter);
		break;
	}
}

//===========================================================================
// DS_Listener
//===========================================================================
void DS_Listener(int property, float value)
{
	switch(property)
	{
	case SFXLP_UNITS_PER_METER:
		unitsPerMeter = value;
		break;

	case SFXLP_DOPPLER:
		alDopplerFactor(value);
		break;
	}
}

//===========================================================================
// SetEnvironment
//===========================================================================
void SetEnvironment(float *rev)
{
/*	A3DREVERB_PROPERTIES rp;
	A3DREVERB_PRESET *pre = &rp.uval.preset;
	float val;

	// Do we already have a reverb object?
	if(a3dReverb == NULL)
	{
		// We need to create it.
		if(FAILED(hr = a3d->NewReverb(&a3dReverb))) 
			return; // Silently go away.
		// Bind it as the current one.
		a3d->BindReverb(a3dReverb);
	}
	memset(&rp, 0, sizeof(rp));
	rp.dwSize = sizeof(rp);
	rp.dwType = A3DREVERB_TYPE_PRESET;
	pre->dwSize = sizeof(A3DREVERB_PRESET);

	val = rev[SRD_SPACE];
	if(rev[SRD_DECAY] > .5)
	{
		// This much decay needs at least the Generic environment.
		if(val < .2) val = .2f;
	}
	pre->dwEnvPreset = val >= 1? A3DREVERB_PRESET_PLAIN
		: val >= .8? A3DREVERB_PRESET_CONCERTHALL
		: val >= .6? A3DREVERB_PRESET_AUDITORIUM
		: val >= .4? A3DREVERB_PRESET_CAVE
		: val >= .2? A3DREVERB_PRESET_GENERIC
		: A3DREVERB_PRESET_ROOM;
	pre->fVolume = rev[SRD_VOLUME];
	pre->fDecayTime = (rev[SRD_DECAY] - .5f) + .55f;
	pre->fDamping = rev[SRD_DAMPING];
	a3dReverb->SetAllProperties(&rp);*/
}

//===========================================================================
// DS_Listenerv
//===========================================================================
void DS_Listenerv(int property, float *values)
{
	float ori[6];

	switch(property)
	{
	case SFXLP_PRIMARY_FORMAT:
		// No need to concern ourselves with this kind of things...
		break;
	
	case SFXLP_POSITION:
		alListener3f(AL_POSITION,
			values[VX]/unitsPerMeter,
			values[VZ]/unitsPerMeter,
			values[VY]/unitsPerMeter);
		break;

	case SFXLP_VELOCITY:
		alListener3f(AL_VELOCITY,
			values[VX]/unitsPerMeter,
			values[VZ]/unitsPerMeter,
			values[VY]/unitsPerMeter);
		break;

	case SFXLP_ORIENTATION:
		Vectors(headYaw = values[VX]/180*PI, 
			headPitch = values[VY]/180*PI, ori, ori + 3);
		alListenerfv(AL_ORIENTATION, ori);
		break;

	case SFXLP_REVERB:
		//SetEnvironment(values);
		break;

	default:
		DS_Listener(property, 0);
	}
}
