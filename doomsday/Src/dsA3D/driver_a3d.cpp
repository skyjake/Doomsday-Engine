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
 * driver_a3d.cpp: A3D Doomsday Sfx Driver
 */

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <objbase.h>
#include <initguid.h>
#include <ia3dapi.h>
#include <stdlib.h>
#include <math.h>

#pragma warning (disable: 4035)

#include "doomsday.h"
#include "sys_sfxd.h"

// MACROS ------------------------------------------------------------------

#define Src(buf)	((IA3dSource2*)buf->ptr)

// TYPES -------------------------------------------------------------------

enum { VX, VY, VZ };

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern "C" {

HRESULT C_DECL	A3dInitialize(void);
void    C_DECL	A3dUninitialize(void);
HRESULT C_DECL	A3dCreate(GUID *,void **,IUnknown FAR *,DWORD); 
HRESULT C_DECL	A3dRegister(void);

}

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

extern "C" {

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

}

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

bool				initOk = false;
int					verbose;
IA3d5				*a3d = 0;
IA3dReverb			*a3dReverb = 0;
IA3dListener		*a3dListener = 0;
A3DCAPS_HARDWARE	hwCaps;
HRESULT				hr;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// Error
//===========================================================================
void Error(const char *where, const char *msg)
{
	Con_Message("%s(A3D): %s [Result = 0x%x]\n", where, msg, hr);
}

//===========================================================================
// DS_Init
//===========================================================================
int DS_Init(void)
{
	HWND hWnd;

	if(initOk) return true;

	// Are we in verbose mode?	
	if((verbose = ArgExists("-verbose")))
		Con_Message("DS_Init(A3D): Starting A3D 3.0...\n");

	// Init A3D.
	if(FAILED(A3dRegister())) return false;
	if(FAILED(A3dInitialize())) return false;
	if(FAILED(hr = A3dCreate(NULL, (void**) &a3d, NULL, A3D_REVERB)))
	{
		Error("DS_Init", "Couldn't create A3D.");
		return false;
	}

	// Get Doomsday's window handle.
	hWnd = (HWND) DD_GetInteger(DD_WINDOW_HANDLE);

	// Set the cooperative level.
	if(FAILED(hr = a3d->SetCooperativeLevel(hWnd, A3D_CL_NORMAL)))
	{
		Error("DS_Init", "Couldn't set cooperative level to normal.");
		return false;
	}

	// Give me the listener!
	if(FAILED(hr = a3d->QueryInterface(IID_IA3dListener, 
		(void**) &a3dListener)))
	{
		Error("DS_Init", "Couldn't get a listener.");
		return false;
	}

	// Set some default settings.
	a3d->SetCoordinateSystem(A3D_LEFT_HANDED_CS);
	a3d->SetUnitsPerMeter(36); // Based on the player sprite.
	a3d->SetDopplerScale(1.5);
	
	memset(&hwCaps, 0, sizeof(hwCaps));
	hwCaps.dwSize = sizeof(hwCaps);
	a3d->GetHardwareCaps(&hwCaps);
	Con_Message("DS_Init(A3D): Number of 3D buffers: %i\n", 
		hwCaps.dwMax3DBuffers);

	a3dReverb = NULL;

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

	if(a3d) a3d->Shutdown(); // This'll get rid of everything.
	a3d = NULL;
	a3dListener = NULL;
	a3dReverb = NULL;
	A3dUninitialize();
	initOk = false;
}

//===========================================================================
// DS_CreateBuffer
//===========================================================================
sfxbuffer_t* DS_CreateBuffer(int flags, int bits, int rate)
{
	IA3dSource2 *src;
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

	// Create the buffer.
	buf = (sfxbuffer_t*) Z_Malloc(sizeof(*buf), PU_STATIC, 0);
	memset(buf, 0, sizeof(*buf));
	buf->ptr = src;
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
	Src(buf)->Release();
	Z_Free(buf);
}

//===========================================================================
// DS_Load
//===========================================================================
void DS_Load(sfxbuffer_t *buf, struct sfxsample_s *sample)
{
	IA3dSource2 *s = Src(buf);

	// Does the buffer already have a sample loaded?
	if(buf->sample)
	{	
		// Is the same one?
		if(buf->sample->id == sample->id) return; 
		
		// Free the existing data.
		s->FreeAudioData();
		buf->sample = NULL;
	}

	// Allocate memory for the sample.
	if(FAILED(hr = s->AllocateAudioData(sample->size)))
	{
		if(verbose)	Error("DS_Load", "Failed to allocate audio data.");
		return; 
	}

	// Copy the sample data into the buffer.
	void *ptr[2];
	DWORD bytes[2];
	if(FAILED(hr = s->Lock(0, 0, &ptr[0], &bytes[0], &ptr[1], &bytes[1],
		A3D_ENTIREBUFFER))) 
	{
		if(verbose) Error("DS_Load", "Failed to lock source.");
		return;
	}
	memcpy(ptr[0], sample->data, bytes[0]);

	// Unlock and we're done.
	s->Unlock(ptr[0], bytes[0], ptr[1], bytes[1]);
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
	Src(buf)->FreeAudioData();
}

//===========================================================================
// DS_Play
//===========================================================================
void DS_Play(sfxbuffer_t *buf)
{
	// Playing is quite impossible without a sample.
	if(!buf->sample) return; 
	Src(buf)->Play(buf->flags & SFXBF_REPEAT? A3D_LOOPED : A3D_SINGLE);
	// The buffer is now playing.
	buf->flags |= SFXBF_PLAYING;
}

//===========================================================================
// DS_Stop
//===========================================================================
void DS_Stop(sfxbuffer_t *buf)
{
	if(!buf->sample) return;
	Src(buf)->Stop();
	Src(buf)->Rewind();
	buf->flags &= ~SFXBF_PLAYING;
}

//===========================================================================
// DS_Refresh
//===========================================================================
void DS_Refresh(sfxbuffer_t *buf)
{
	IA3dSource2 *s = Src(buf);
	DWORD status;
	
	// Has the buffer finished playing?
	s->GetStatus(&status);
	if(!status)
	{
		// It has stopped playing.
		buf->flags &= ~SFXBF_PLAYING;
	}
}

//===========================================================================
// DS_Event
//===========================================================================
void DS_Event(int type)
{
	switch(type)
	{
	case SFXEV_BEGIN:
		a3d->Clear();
		break;

	case SFXEV_END:
		a3d->Flush();
		break;
	}
}

//===========================================================================
// SetPan
//	Pan is linear, from -1 to 1. 0 is in the middle.
//===========================================================================
void SetPan(IA3dSource2 *source, float pan)
{
	float gains[2];
	
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
	source->SetPanValues(2, gains);
}

//===========================================================================
// DS_Set
//===========================================================================
void DS_Set(sfxbuffer_t *buf, int property, float value)
{
	DWORD dw;
	IA3dSource2 *s = Src(buf);
	float minDist, maxDist;

	switch(property)
	{
	case SFXBP_VOLUME:
		s->SetGain(value);
		break;

	case SFXBP_FREQUENCY:
		dw = DWORD(buf->rate * value);
		if(dw != buf->freq)	// Don't set redundantly.
		{
			buf->freq = dw;
			s->SetPitch(value);
		}
		break;

	case SFXBP_PAN:
		SetPan(s, value);
		break;

	case SFXBP_MIN_DISTANCE:
		s->GetMinMaxDistance(&minDist, &maxDist, &dw);
		s->SetMinMaxDistance(value, maxDist, A3D_MUTE);
		break;

	case SFXBP_MAX_DISTANCE:
		s->GetMinMaxDistance(&minDist, &maxDist, &dw);
		s->SetMinMaxDistance(minDist, value, A3D_MUTE);
		break;

	case SFXBP_RELATIVE_MODE:
		s->SetTransformMode(value? A3DSOURCE_TRANSFORMMODE_HEADRELATIVE
			: A3DSOURCE_TRANSFORMMODE_NORMAL);
		break;
	}
}

//===========================================================================
// DS_Setv
//===========================================================================
void DS_Setv(sfxbuffer_t *buf, int property, float *values)
{
	IA3dSource2 *s = Src(buf);

	switch(property)
	{
	case SFXBP_POSITION:
		s->SetPosition3f(values[VX], values[VZ], values[VY]);
		break;

	case SFXBP_VELOCITY:
		s->SetVelocity3f(values[VX], values[VZ], values[VY]);
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
		a3d->SetUnitsPerMeter(value);
		break;

	case SFXLP_DOPPLER:
		a3d->SetDopplerScale(value);
		break;
	}
}

//===========================================================================
// SetEnvironment
//===========================================================================
void SetEnvironment(float *rev)
{
	A3DREVERB_PROPERTIES rp;
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
	a3dReverb->SetAllProperties(&rp);
}

//===========================================================================
// DS_Listenerv
//===========================================================================
void DS_Listenerv(int property, float *values)
{
	switch(property)
	{
	case SFXLP_PRIMARY_FORMAT:
		// No need to concern ourselves with this kind of things...
		break;
	
	case SFXLP_POSITION:
		a3dListener->SetPosition3f(values[VX], values[VZ], values[VY]);
		break;

	case SFXLP_VELOCITY:
		a3dListener->SetVelocity3f(values[VX], values[VZ], values[VY]);
		break;

	case SFXLP_ORIENTATION:
		a3dListener->SetOrientationAngles3f(-values[VX] + 90, values[VY], 0);
		break;

	case SFXLP_REVERB:
		SetEnvironment(values);
		break;

	default:
		DS_Listener(property, 0);
	}
}
