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
 * driver_sndcmp.cpp: Maximum Compatibility Sfx Driver
 *
 * Buffers created on Load.
 */

// HEADER FILES ------------------------------------------------------------

#define DIRECTSOUND_VERSION 0x0600		// Uses DirectSound 6.0

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <eax.h>
#include <math.h>
#include <stdlib.h>

#pragma warning (disable: 4035 4244)

#include "doomsday.h"
#include "sys_sfxd.h"

// MACROS ------------------------------------------------------------------

// DirectSound(3D)Buffer Pointer
#define DSBuf(buf)		((LPDIRECTSOUNDBUFFER) buf->ptr)
#define DSBuf3(buf)		((LPDIRECTSOUND3DBUFFER) buf->ptr3d)

#define NEEDED_SUPPORT	( KSPROPERTY_SUPPORT_GET | KSPROPERTY_SUPPORT_SET )

#define PI				3.14159265

// TYPES -------------------------------------------------------------------

enum { VX, VY, VZ };

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

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

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

bool					initOk = false;
int						verbose;
HRESULT					hr;
LPDIRECTSOUND			dsound;
LPDIRECTSOUNDBUFFER		primary;
LPDIRECTSOUND3DLISTENER	dsListener;
LPKSPROPERTYSET			eaxListener;
DSCAPS					dsCaps;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// Error
//===========================================================================
void Error(const char *where, const char *msg)
{
	Con_Message("%s(Compat): %s [Result = 0x%x]\n", where, msg, hr);
}

//===========================================================================
// CreateDSBuffer
//===========================================================================
int CreateDSBuffer(DWORD flags, int samples, int freq, int bits, int channels, 
				   LPDIRECTSOUNDBUFFER *bufAddr)
{
	DSBUFFERDESC	bufd;
	WAVEFORMATEX	form;
	DWORD			dataBytes = samples * bits/8 * channels;
	
	// Prepare the buffer description.
	memset(&bufd, 0, sizeof(bufd));
	bufd.dwSize = sizeof(bufd);
	bufd.dwFlags = flags;
	bufd.dwBufferBytes = dataBytes;
	bufd.lpwfxFormat = &form;

	// Prepare the format description.
	memset(&form, 0, sizeof(form));
	form.wFormatTag = WAVE_FORMAT_PCM;
	form.nChannels = channels;
	form.nSamplesPerSec = freq;
	form.nBlockAlign = channels * bits/8;
	form.nAvgBytesPerSec = form.nSamplesPerSec * form.nBlockAlign;
	form.wBitsPerSample = bits;

	return dsound->CreateSoundBuffer(&bufd, bufAddr, NULL);
}

//===========================================================================
// FreeDSBuffers
//===========================================================================
void FreeDSBuffers(sfxbuffer_t *buf)
{
	if(DSBuf3(buf)) DSBuf3(buf)->Release();
	if(DSBuf(buf)) DSBuf(buf)->Release();
	buf->ptr = buf->ptr3d = NULL;
}

//===========================================================================
// DS_Init
//===========================================================================
int DS_Init(void)
{
	HWND				hWnd;
	DSBUFFERDESC		desc;
	LPDIRECTSOUNDBUFFER	bufTemp;

	if(initOk) return true;

	// Are we in verbose mode?	
	if((verbose = ArgExists("-verbose")))
		Con_Message("DS_Init(Compat): Initializing sound driver...\n");

	// Get Doomsday's window handle.
	hWnd = (HWND) DD_GetInteger(DD_WINDOW_HANDLE);

	hr = DS_OK;
	if(ArgExists("-noeax") 
		|| FAILED(hr = EAXDirectSoundCreate(NULL, &dsound, NULL)))
	{
		// EAX can't be initialized. Use normal DS, then.
		if(FAILED(hr)) Error("DS_Init", "EAX 2 couldn't be initialized.");
		if(FAILED(hr = DirectSoundCreate(NULL, &dsound, NULL)))
		{
			Error("DS_Init", "Failed to create dsound interface.");
			return false;
		}
	}
	// Set the cooperative level.
	if(FAILED(hr = dsound->SetCooperativeLevel(hWnd, DSSCL_PRIORITY)))
	{
		Error("DS_Init", "Couldn't set dSound coop level.");
		return false;
	}
	// Get the primary buffer and the listener.
	primary = NULL;
	memset(&desc, 0, sizeof(desc));
	desc.dwSize = sizeof(desc);
	desc.dwFlags = DSBCAPS_CTRL3D | DSBCAPS_PRIMARYBUFFER;
	dsListener = NULL;
	if(SUCCEEDED(dsound->CreateSoundBuffer(&desc, &primary, NULL)))
	{
		// Query the listener interface.
		primary->QueryInterface(IID_IDirectSound3DListener, 
			(void**) &dsListener);
	}
	else
	{
		// Failure; get a 2D primary buffer, then.
		desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
		dsound->CreateSoundBuffer(&desc, &primary, NULL);
	}
	// Start playing the primary buffer.
	if(primary) 
	{
		if(FAILED(hr = primary->Play(0, 0, DSBPLAY_LOOPING)))
			Error("DS_Init", "Can't play primary buffer.");
	}
	
	// Try to get the EAX listener property set. 
	// Create a temporary secondary buffer for it.
	eaxListener = NULL;
	if(SUCCEEDED(CreateDSBuffer(DSBCAPS_STATIC | DSBCAPS_CTRL3D, 
		DSBSIZE_MIN, 22050, 8, 1, &bufTemp)))
	{
		// Now try to get the property set.
		if(SUCCEEDED(hr = bufTemp->QueryInterface(IID_IKsPropertySet, 
			(void**) &eaxListener)))
		{
			DWORD support = 0, revsize = 0;
			// Check for support.
			if(FAILED(hr = eaxListener->QuerySupport(
				DSPROPSETID_EAX_ListenerProperties,
				DSPROPERTY_EAXLISTENER_ENVIRONMENT,
				&support)) 
				|| ((support & NEEDED_SUPPORT) != NEEDED_SUPPORT))
			{
				Error("DS_Init", "Sufficient EAX2 support not present.");
				eaxListener->Release();
				eaxListener = NULL;
			}
			else
			{
				// EAX is supported!
				if(verbose) Con_Message("DS_Init(Compat): EAX2 is available.\n");
			}
		}
		// Release the temporary buffer interface.
		bufTemp->Release();
	}
		
	// Get the caps.
	dsCaps.dwSize = sizeof(dsCaps);
	dsound->GetCaps(&dsCaps);
	if(verbose) Con_Message("DS_Init(Compat): Number of hardware 3D "
		"buffers: %i\n", dsCaps.dwMaxHw3DAllBuffers);

	// Configure the DS3D listener.
	if(dsListener)
	{
		dsListener->SetDistanceFactor(1/36.0f, DS3D_DEFERRED);
		dsListener->SetDopplerFactor(2, DS3D_DEFERRED);
	}

	// Success!
	initOk = true;
	return true;
}

//===========================================================================
// DS_Shutdown
//===========================================================================
void DS_Shutdown(void)
{
	if(!initOk) return;

	if(eaxListener) eaxListener->Release();
	if(dsListener) dsListener->Release();
	if(primary) primary->Release();
	if(dsound) dsound->Release();
	
	eaxListener = NULL;
	dsListener = NULL;
	primary = NULL;
	dsound = NULL;

	initOk = false;
}

//===========================================================================
// DS_CreateBuffer
//===========================================================================
sfxbuffer_t* DS_CreateBuffer(int flags, int bits, int rate)
{
	sfxbuffer_t *buf;

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
	src->SetAudioFormat((void*)f);*/

	// Attempt to create a new buffer.
	/*if(FAILED(hr = CreateDSBuffer(
		DSBCAPS_CTRLFREQUENCY | DSBCAPS_CTRLVOLUME 		
		| (flags & SFXBF_3D? DSBCAPS_CTRL3D : DSBCAPS_CTRLPAN), */

	// Since we don't know how long the buffer must be, we'll just create
	// an sfxbuffer_t with no DSound buffer attached. The DSound buffer
	// will be created when a sample is loaded.

	buf = (sfxbuffer_t*) Z_Calloc(sizeof(*buf), PU_STATIC, 0);
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
	FreeDSBuffers(buf);
	Z_Free(buf);
}

//===========================================================================
// DS_Load
//===========================================================================
void DS_Load(sfxbuffer_t *buf, struct sfxsample_s *sample)
{
#define SAMPLE_SILENCE	16		// Samples to interpolate to silence.
#define SAMPLE_ROUNDOFF	32		// The length is a multiple of this.
	LPDIRECTSOUNDBUFFER newSound = NULL;
	LPDIRECTSOUND3DBUFFER newSound3D = NULL;
	bool play3d = !!(buf->flags & SFXBF_3D);
	void *writePtr1 = NULL, *writePtr2 = NULL;
	DWORD writeBytes1, writeBytes2;
	unsigned int safeNumSamples = sample->numsamples + SAMPLE_SILENCE;
	unsigned int safeSize, i;
	int last, delta;

	// Does the buffer already have a sample loaded?
	if(buf->sample)
	{	
		// Is the same one?
		if(buf->sample->id == sample->id) return; 
	}

	// The safe number of samples is rounded to the next highest 
	// count of SAMPLE_ROUNDOFF.
	if((i = safeNumSamples % SAMPLE_ROUNDOFF) != 0)
	{
		safeNumSamples += SAMPLE_ROUNDOFF - i;
/*#if _DEBUG
		Con_Message("Safelen = %i\n", safeNumSamples);
#endif*/
	}
	safeSize = safeNumSamples * sample->bytesper;
/*#if _DEBUG
	Con_Message("Safesize = %i\n", safeSize);
#endif*/

	// If a sample has already been loaded, unload it.
	FreeDSBuffers(buf);

	// Create the DirectSound buffer. Its length will match the sample 
	// exactly.
	if(FAILED(hr = CreateDSBuffer(DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY 
		| DSBCAPS_STATIC | (play3d? DSBCAPS_CTRL3D 
		| DSBCAPS_MUTE3DATMAXDISTANCE : DSBCAPS_CTRLPAN),
		safeNumSamples, buf->freq, buf->bytes*8, 1, &newSound)))
	{
		if(verbose) Error("DS_Load", "Couldn't create a new buffer.");
		return;
	}
	if(play3d)
	{
		// Query the 3D interface.
		if(FAILED(hr = newSound->QueryInterface(IID_IDirectSound3DBuffer, 
			(void**) &newSound3D)))
		{
			if(verbose) Error("DS_Load", "Couldn't get 3D buffer interface.");
			newSound->Release();
			return;
		}
	}

	// Lock and load!
	newSound->Lock(0, 0, &writePtr1, &writeBytes1, 
		&writePtr2, &writeBytes2, DSBLOCK_ENTIREBUFFER);
	if(writePtr2 && verbose) Error("DS_Load", "Unexpected buffer lock behavior.");

	// Copy the sample data.
	memcpy(writePtr1, sample->data, sample->size);

	/*Con_Message("Fill %i bytes (orig=%i).\n", safeSize - sample->size,
			sample->size);*/

	// Interpolate to silence.
	// numSafeSamples includes at least SAMPLE_ROUNDOFF extra samples.
	last = sample->bytesper==1? ((byte*)sample->data)[sample->numsamples - 1]
		: ((short*)sample->data)[sample->numsamples - 1];
	delta = sample->bytesper==1? 0x80 - last : -last;
	//Con_Message("last = %i\n", last);
	for(i = 0; i < safeNumSamples - sample->numsamples; i++)
	{
		float pos = i/(float)SAMPLE_SILENCE;
		if(pos > 1) pos = 1;
		if(sample->bytesper == 1) // 8-bit sample.
		{
			((byte*)writePtr1)[sample->numsamples + i] = 
				byte( last + delta*pos );
		}
		else // 16-bit sample.
		{
			((short*)writePtr1)[sample->numsamples + i] = 
				short( last + delta*pos );
		}
	}

	// Unlock the buffer.
	newSound->Unlock(writePtr1, writeBytes1, writePtr2, writeBytes2);

	buf->ptr = newSound;
	buf->ptr3d = newSound3D;
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
	FreeDSBuffers(buf);
}

//===========================================================================
// DS_Play
//===========================================================================
void DS_Play(sfxbuffer_t *buf)
{
	// Playing is quite impossible without a sample.
	if(!buf->sample || !DSBuf(buf)) return; 
	DSBuf(buf)->SetCurrentPosition(0);
	DSBuf(buf)->Play(0, 0, buf->flags & SFXBF_REPEAT? DSBPLAY_LOOPING : 0);
	// The buffer is now playing.
	buf->flags |= SFXBF_PLAYING;
}

//===========================================================================
// DS_Stop
//===========================================================================
void DS_Stop(sfxbuffer_t *buf)
{
	if(!buf->sample || !DSBuf(buf)) return;
	DSBuf(buf)->Stop();
	buf->flags &= ~SFXBF_PLAYING;
}

//===========================================================================
// DS_Refresh
//===========================================================================
void DS_Refresh(sfxbuffer_t *buf)
{
	LPDIRECTSOUNDBUFFER sound = DSBuf(buf);
	DWORD status;

	if(sound)
	{
		// Has the buffer finished playing?
		sound->GetStatus(&status);
		if(!(status & DSBSTATUS_PLAYING)
			&& buf->flags & SFXBF_PLAYING)
		{
			// It has stopped playing.
			buf->flags &= ~SFXBF_PLAYING;
		}
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
		//a3d->Clear();
		break;

	case SFXEV_END:
		//a3d->Flush();
		break;
	}
}

//===========================================================================
// LinLog
//	Convert linear volume 0..1 to logarithmic -10000..0.
//===========================================================================
int LinLog(float vol)
{
	int ds_vol;

	if(vol <= 0) return DSBVOLUME_MIN;
	if(vol >= 1) return DSBVOLUME_MAX;
	
	// Straighten the volume curve.
	ds_vol = int(100 * 20 * log10(vol));
	if(ds_vol < DSBVOLUME_MIN) ds_vol = DSBVOLUME_MIN;
	return ds_vol;
}

//===========================================================================
// LogPan
//	Convert linear pan -1..1 to logarithmic -10000..10000.
//===========================================================================
int LogPan(float pan)
{
	if(pan >= 1) return DSBPAN_RIGHT;
	if(pan <= -1) return DSBPAN_LEFT;
	if(pan == 0) return 0;
	if(pan > 0) return int(-100 * 20 * log10(1 - pan));
	return int(100 * 20 * log10(1 + pan));
}

//===========================================================================
// DS_Set
//	SFXBP_VOLUME (if negative, interpreted as attenuation)
//	SFXBP_FREQUENCY
//	SFXBP_PAN (-1..1)
//	SFXBP_MIN_DISTANCE
//	SFXBP_MAX_DISTANCE
//	SFXBP_RELATIVE_MODE
//===========================================================================
void DS_Set(sfxbuffer_t *buf, int property, float value)
{
	unsigned int f;
	LPDIRECTSOUNDBUFFER sound = DSBuf(buf);

	if(!sound) return;
	switch(property)
	{
	case SFXBP_VOLUME:
		// Use logarithmic attenuation.
		sound->SetVolume(value <= 0? (-1-value) * 10000
			: LinLog(value));	// Linear volume.
		break;

	case SFXBP_FREQUENCY:
		f = unsigned int(buf->rate * value);
		// Don't set redundantly.
		if(f != buf->freq) sound->SetFrequency(buf->freq = f);
		break;

	case SFXBP_PAN:
		sound->SetPan(LogPan(value));
		break;

	case SFXBP_MIN_DISTANCE:
		if(!DSBuf3(buf)) return;
		DSBuf3(buf)->SetMinDistance(value, DS3D_DEFERRED);
		break;

	case SFXBP_MAX_DISTANCE:
		if(!DSBuf3(buf)) return;
		DSBuf3(buf)->SetMaxDistance(value, DS3D_DEFERRED);
		break;

	case SFXBP_RELATIVE_MODE:
		if(!DSBuf3(buf)) return;
		DSBuf3(buf)->SetMode(value? DS3DMODE_HEADRELATIVE : DS3DMODE_NORMAL,
			DS3D_DEFERRED);
		break;
	}
}

//===========================================================================
// DS_Setv
//	SFXBP_POSITION
//	SFXBP_VELOCITY
//	Coordinates specified in world coordinate system, converted to DSound's: 
//	+X to the right, +Y up and +Z away (Y and Z swapped, i.e.).
//===========================================================================
void DS_Setv(sfxbuffer_t *buf, int property, float *values)
{
	if(!DSBuf3(buf)) return;

	switch(property)
	{
	case SFXBP_POSITION:
		IDirectSound3DBuffer_SetPosition(DSBuf3(buf),
			values[VX], values[VZ], values[VY], DS3D_DEFERRED);
		break;

	case SFXBP_VELOCITY:
		IDirectSound3DBuffer_SetVelocity(DSBuf3(buf),
			values[VX], values[VZ], values[VY], DS3D_DEFERRED);
		break;
	}
}

//===========================================================================
// EAXCommitDeferred
//===========================================================================
void EAXCommitDeferred(void)
{
	if(!eaxListener) return;
	eaxListener->Set(DSPROPSETID_EAX_ListenerProperties,
		DSPROPERTY_EAXLISTENER_COMMITDEFERREDSETTINGS, 
		NULL, 0, NULL, 0);
}

//===========================================================================
// DS_DSoundListenerOrientation
//	Parameters are in radians. 
//	Example front vectors: 
//	  Yaw 0:(0,0,1), pi/2:(-1,0,0)
//===========================================================================
void ListenerOrientation(float yaw, float pitch)
{
	float front[3], up[3];

	if(!dsListener) return;

	front[VX] = cos(yaw) * cos(pitch);
	front[VZ] = sin(yaw) * cos(pitch);
	front[VY] = sin(pitch);

	up[VX] = -cos(yaw) * sin(pitch);
	up[VZ] = -sin(yaw) * sin(pitch);
	up[VY] = cos(pitch);

	dsListener->SetOrientation(front[VX], front[VY], front[VZ],
		up[VX],	up[VY],	up[VZ],	DS3D_DEFERRED);
}

//===========================================================================
// EAXSetdw
//===========================================================================
void EAXSetdw(DWORD prop, int value)
{
	if(!eaxListener) return;
	eaxListener->Set(DSPROPSETID_EAX_ListenerProperties,
		prop | DSPROPERTY_EAXLISTENER_DEFERRED, NULL, 0, 
		&value, sizeof(DWORD));
}

//===========================================================================
// EAXSetf
//===========================================================================
void EAXSetf(DWORD prop, float value)
{
	if(!eaxListener) return;
	eaxListener->Set(DSPROPSETID_EAX_ListenerProperties,
		prop | DSPROPERTY_EAXLISTENER_DEFERRED, NULL, 0, 
		&value, sizeof(float));
}

//===========================================================================
// EAXMuldw
//	Linear multiplication for a logarithmic property.
//===========================================================================
void EAXMuldw(DWORD prop, float mul)
{
	DWORD	retBytes;
	LONG	value;

	if(!eaxListener) return;
	if(FAILED(hr = eaxListener->Get(DSPROPSETID_EAX_ListenerProperties,
		prop, NULL, 0, &value, sizeof(value), &retBytes)))
	{
		/*if(!ignore_eax_errors)
			Con_Message("DS_EAXMuldw (prop:%i) get failed. Result: %x.\n", 
				prop, hr & 0xffff);*/
		return;
	}
	EAXSetdw(prop, LinLog(pow(10, value/2000.0f) * mul));
}

//===========================================================================
// EAXMulf
//	Linear multiplication for a linear property.
//===========================================================================
void EAXMulf(DWORD prop, float mul, float min, float max)
{
	DWORD retBytes;
	float value;

	if(!eaxListener) return;
	if(FAILED(hr = eaxListener->Get(DSPROPSETID_EAX_ListenerProperties,
		prop, NULL, 0, &value, sizeof(value), &retBytes)))
	{
/*		if(!ignore_eax_errors)
			Con_Message("DS_EAXMulf (prop:%i) get failed. Result: %x.\n", 
				prop, hr & 0xffff);*/
		return;
	}
	value *= mul;
	if(value < min) value = min;
	if(value > max) value = max;
	EAXSetf(prop, value);
}

//===========================================================================
// DS_Listener
//	SFXLP_UNITS_PER_METER
//	SFXLP_DOPPLER
//	SFXLP_UPDATE
//===========================================================================
void DS_Listener(int property, float value)
{
	if(!dsListener) return;

	switch(property)
	{
	case SFXLP_UPDATE:
		// Commit any deferred settings.
		dsListener->CommitDeferredSettings();
		EAXCommitDeferred();
		break;

	case SFXLP_UNITS_PER_METER:
		dsListener->SetDistanceFactor(1/value, DS3D_IMMEDIATE);
		break;

	case SFXLP_DOPPLER:
		dsListener->SetDopplerFactor(value, DS3D_IMMEDIATE);
		break;
	}
}

//===========================================================================
// ListenerEnvironment
//	Values use SRD_* for indices.
//===========================================================================
void ListenerEnvironment(float *rev)
{
	float val;

	// This can only be done if EAX is available.
	if(!eaxListener) return; 
	
	val = rev[SRD_SPACE];
	if(rev[SRD_DECAY] > .5)
	{
		// This much decay needs at least the Generic environment.
		if(val < .2) val = .2f;
	}

	// Set the environment. Other properties are updated automatically.
	EAXSetdw(DSPROPERTY_EAXLISTENER_ENVIRONMENT,
			val >= 1? EAX_ENVIRONMENT_PLAIN
			: val >= .8? EAX_ENVIRONMENT_CONCERTHALL
			: val >= .6? EAX_ENVIRONMENT_AUDITORIUM
			: val >= .4? EAX_ENVIRONMENT_CAVE
			: val >= .2? EAX_ENVIRONMENT_GENERIC
			: EAX_ENVIRONMENT_ROOM);

	// General reverb volume adjustment.
	EAXSetdw(DSPROPERTY_EAXLISTENER_ROOM, LinLog(rev[SRD_VOLUME]));

	// Reverb decay.
	val = (rev[SRD_DECAY] - .5)*1.5 + 1;
	EAXMulf(DSPROPERTY_EAXLISTENER_DECAYTIME, val, 
		EAXLISTENER_MINDECAYTIME, EAXLISTENER_MAXDECAYTIME);

	// Damping.
	val = 1.1f * (1.2f - rev[SRD_DAMPING]);
	if(val < .1) val = .1f;
	EAXMuldw(DSPROPERTY_EAXLISTENER_ROOMHF, val);

	// A slightly increased roll-off.
	EAXSetf(DSPROPERTY_EAXLISTENER_ROOMROLLOFFFACTOR, 1.3f);
}

//===========================================================================
// DS_Listenerv
//	Call SFXLP_UPDATE at the end of every channel update.
//===========================================================================
void DS_Listenerv(int property, float *values)
{
	if(!dsListener) return;

	switch(property)
	{
	case SFXLP_POSITION:
		dsListener->SetPosition(values[VX], values[VZ], values[VY], 
			DS3D_DEFERRED);
		break;

	case SFXLP_VELOCITY:
		dsListener->SetVelocity(values[VX], values[VZ], values[VY], 
			DS3D_DEFERRED);
		break;

	case SFXLP_ORIENTATION:
		ListenerOrientation(values[VX]/180*PI, values[VY]/180*PI);
		break;

	case SFXLP_REVERB:
		ListenerEnvironment(values);
		break;

	default:
		DS_Listener(property, 0);
	}
}



#if 0
// i_DS_EAX.c
// Playing sounds using DirectSound(3D) and EAX, if available.
// Author: Jaakko Keränen

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <eax.h>
#include <math.h>
#include "dd_def.h"
#include "i_win32.h"
#include "i_timer.h"
#include "i_sound.h"
#include "settings.h"

// MACROS -------------------------------------------------------------------

#define MAX_SND_DIST		2025

/*#define SBUFPTR(handle)		(sources+handle-1)
#define SBUFHANDLE(sbptr)	((sbptr-sources)+1)*/

#define NEEDED_SUPPORT		( KSPROPERTY_SUPPORT_GET | KSPROPERTY_SUPPORT_SET )

/*#define MAX_PLAYSTACK		16
#define MAX_SOUNDS_PER_FRAME 3*/

/*#ifndef VZ
enum { VX, VY, VZ };
#endif*/

// TYPES --------------------------------------------------------------------

typedef struct
{
	int						id;
	LPDIRECTSOUNDBUFFER		source;
	LPDIRECTSOUND3DBUFFER	source3D;
	int						freq;	// The original frequence of the sample.
	int						startTime;
} sndsource_t;

typedef struct
{
	unsigned short	bitsShift;
	unsigned short	frequency;
	unsigned short	length;
	unsigned short	reserved;
} sampleheader_t;

// PRIVATE FUNCTIONS --------------------------------------------------------

void I2_DestroyAllSources();
int I2_PlaySound(void *data, boolean play3d, sound3d_t *desc, int pan);
int CreateDSBuffer(DWORD flags, int samples, int freq, int bits, int channels, 
				   LPDIRECTSOUNDBUFFER *bufAddr);

// EXTERNAL DATA ------------------------------------------------------------

extern HWND hWndMain;

// PUBLIC DATA --------------------------------------------------------------

// PRIVATE DATA -------------------------------------------------------------

static int						idGen = 0;
static boolean					initOk = false;
static LPDIRECTSOUND			dsound = NULL;
static LPDIRECTSOUND3DLISTENER	dsListener = NULL;
static LPKSPROPERTYSET			eaxListener = NULL;
static HRESULT					hr;
static DSCAPS					dsCaps;

static sndsource_t				*sources = 0;
static int						numSources = 0;

/*
static playstack_t				playstack[MAX_PLAYSTACK];
static int						playstackpos = 0;
static int						soundsPerTick = 0;
*/

static float					listenerYaw = 0, listenerPitch = 0;

// CODE ---------------------------------------------------------------------

int I2_Init()
{
	DSBUFFERDESC		desc;
	LPDIRECTSOUNDBUFFER	bufTemp;

	if(initOk) return true;	// Don't init a second time.
	if(FAILED(hr = EAXDirectSoundCreate(NULL, &dsound, NULL)))
	{
		// EAX can't be initialized. Use normal DS, then.
		ST_Message("I2_Init: EAX 2 couldn't be initialized (result: %i).\n", hr & 0xffff);
		if(FAILED(hr = DirectSoundCreate(NULL, &dsound, NULL)))
		{
			ST_Message("I2_Init: Couldn't create dsound (result: %i).\n", hr & 0xffff);
			return false;
		}
	}
	// Set the cooperative level.
	if(FAILED(hr = IDirectSound_SetCooperativeLevel(dsound, hWndMain, DSSCL_PRIORITY)))
	{
		ST_Message("I2_Init: Couldn't set dSound cooperative level (result: %i).\n", hr & 0xffff);
		return false;
	}
	// Get the listener.
	memset(&desc, 0, sizeof(desc));
	desc.dwSize = sizeof(desc);
	desc.dwFlags = DSBCAPS_CTRL3D | DSBCAPS_PRIMARYBUFFER;
	if(SUCCEEDED(IDirectSound_CreateSoundBuffer(dsound, &desc, &bufTemp, NULL)))
	{
		// Query the listener interface.
		IDirectSoundBuffer_QueryInterface(bufTemp, &IID_IDirectSound3DListener, &dsListener);
	}
	// Release the primary buffer interface, we won't need it.
	IDirectSoundBuffer_Release(bufTemp);
	
	// Try to get the EAX listener property set. Create a temporary secondary buffer for it.
	if(SUCCEEDED( CreateDSBuffer(DSBCAPS_STATIC | DSBCAPS_CTRL3D, DSBSIZE_MIN, 22050, 8, 1, &bufTemp) ))
	{
		// Now try to get the property set.
		if(SUCCEEDED(hr = IDirectSoundBuffer_QueryInterface(bufTemp, 
			&IID_IKsPropertySet, &eaxListener)))
		{
			DWORD support = 0, revsize = 0;
			// Check for support.
			if(FAILED(hr = IKsPropertySet_QuerySupport(eaxListener,
				&DSPROPSETID_EAX_ListenerProperties,
				DSPROPERTY_EAXLISTENER_ENVIRONMENT,
				&support)) 
				|| ((support & NEEDED_SUPPORT) != NEEDED_SUPPORT))
			{
				ST_Message("I2_Init: Property set acquired, but EAX 2 not supported.\n  Result:%i, support:%x\n", hr&0xffff, support);
				IKsPropertySet_Release(eaxListener);
				eaxListener = NULL;
			}
			else
			{
				// EAX is supported!
				ST_Message("I2_Init: EAX 2 available.\n");
			}
		}
		// Release the temporary buffer interface.
		IDirectSoundBuffer_Release(bufTemp);
	}
		
	// Get the caps.
	dsCaps.dwSize = sizeof(dsCaps);
	IDirectSound_GetCaps(dsound, &dsCaps);
	ST_Message("I2_Init: Number of hardware 3D buffers: %i\n", dsCaps.dwMaxHw3DAllBuffers);

	// Configure the DS3D listener.
	if(dsListener)
	{
		IDirectSound3DListener_SetDistanceFactor(dsListener, 1/36.0f, DS3D_DEFERRED);
		IDirectSound3DListener_SetDopplerFactor(dsListener, 2, DS3D_DEFERRED);
	}

	// Success!
	initOk = true;
	return true;
}

void I2_Shutdown()
{
	if(!initOk) return;

	I2_DestroyAllSources();

	// Release the DS objects.
	if(eaxListener) IKsPropertySet_Release(eaxListener);
	if(dsListener) IDirectSound3DListener_Release(dsListener);
	if(dsound) IDirectSound_Release(dsound);
	eaxListener = NULL;
	dsListener = NULL;
	dsound = NULL;
}

static sndsource_t *GetSource(int handle)
{
	int i;
	
	for(i=0; i<numSources; i++)
		if(sources[i].id == handle)
			return sources + i;
	return NULL;
}

// Converts the linear 0 - 1 to the logarithmic -10000 - 0.
static int LinLog(float zero_to_one)
{
	int val;
	if(zero_to_one <= 0) return -10000;
	if(zero_to_one >= 1) return 0;
	val = 2000 * log10(zero_to_one);	
	if(val < -10000) val = -10000;
	return val;
}

void EAX_dwSet(DWORD prop, int value)
{
	if(FAILED(hr = IKsPropertySet_Set(eaxListener, 
		&DSPROPSETID_EAX_ListenerProperties,
		prop | DSPROPERTY_EAXLISTENER_DEFERRED, NULL, 0, &value, sizeof(int))))
	{
		I_Error("EAX_dwSet (prop:%i value:%i) failed. Result: %i.\n", 
			prop, value, hr&0xffff);
	}
}

void EAX_fSet(DWORD prop, float value)
{
	if(FAILED(hr = IKsPropertySet_Set(eaxListener, 
		&DSPROPSETID_EAX_ListenerProperties,
		prop | DSPROPERTY_EAXLISTENER_DEFERRED, NULL, 0, &value, sizeof(float))))
	{
		I_Error("EAX_fSet (prop:%i value:%f) failed. Result: %i.\n", 
			prop, value, hr&0xffff);
	}
}

void EAX_dwMul(DWORD prop, float mul)
{
	DWORD	retBytes;
	LONG	value;

	if(FAILED(hr = IKsPropertySet_Get(eaxListener, &DSPROPSETID_EAX_ListenerProperties,
		prop, NULL, 0, &value, sizeof(value), &retBytes)))
	{
		I_Error("EAX_fMul (prop:%i) get failed. Result: %i.\n", prop, hr & 0xffff);
	}
	EAX_dwSet(prop, LinLog(pow(10, value/2000.0f) * mul));
}

void EAX_fMul(DWORD prop, float mul, float min, float max)
{
	DWORD retBytes;
	float value;

	if(FAILED(hr = IKsPropertySet_Get(eaxListener, &DSPROPSETID_EAX_ListenerProperties,
		prop, NULL, 0, &value, sizeof(value), &retBytes)))
	{
		I_Error("EAX_fMul (prop:%i) get failed. Result: %i.\n", prop, hr & 0xffff);
	}
	value *= mul;
	if(value < min) value = min;
	if(value > max) value = max;
	EAX_fSet(prop, value);
}

void EAX_CommitDeferred()
{
	if(!eaxListener) return;
	if(FAILED(IKsPropertySet_Set(eaxListener, &DSPROPSETID_EAX_ListenerProperties,
		DSPROPERTY_EAXLISTENER_COMMITDEFERREDSETTINGS, NULL, 0,
		NULL, 0)))
	{
		I_Error("EAX_CommitDeferred failed.\n");
	}
}

void I2_BeginSoundFrame()
{
/*	int		i;

	if(!initOk) return;

	soundsPerFrame = 0;
	// Play sounds on the stack.
	for(i=0; i<MAX_SOUNDS_PER_FRAME && playstackpos > 0; i++, soundsPerFrame++)
	{
		playstack_t *pst = playstack + playstackpos-1;
		I2_PlaySound(pst->data, pst->play3d, &pst->desc, pst->pan);
	}*/
}

void I2_EndSoundFrame()
{
	if(!initOk || !dsListener) return;
	IDirectSound3DListener_CommitDeferredSettings(dsListener);
}

void I2_KillSource(sndsource_t *src)
{
	if(src->source3D) IDirectSound3DBuffer_Release(src->source3D);
	if(src->source) IDirectSoundBuffer_Release(src->source);
	memset(src, 0, sizeof(*src));	
}

int I_Play2DSound(void *data, int volume, int pan, int pitch)
{
	sound3d_t desc;
	int handle;

	desc.flags = DDSOUNDF_VOLUME | DDSOUNDF_PITCH;
	desc.volume = volume;
	desc.pitch = pitch;
	handle = I2_PlaySound(data, false, &desc, pan);
	return handle;
}

int I_Play3DSound(void *data, sound3d_t *desc)
{
	return I2_PlaySound(data, true, desc, 0);
}

void I_StopSound(int handle)
{
	sndsource_t *buf = GetSource(handle);

	if(!initOk || !buf) return;
	if(buf->source)
	{
		IDirectSoundBuffer_Stop(buf->source);
		IDirectSoundBuffer_SetCurrentPosition(buf->source, 0);
	}
}

void I_UpdateListener(listener3d_t *desc)
{
	float	temp[3], val; 
	int		i;

	if(!initOk || !dsListener || !desc) return;
	if(desc->flags & DDLISTENERF_POS)
	{
		for(i=0; i<3; i++) temp[i] = FIX2FLT(desc->pos[i]);
		IDirectSound3DListener_SetPosition(dsListener, temp[VX], temp[VY], temp[VZ], DS3D_DEFERRED);
	}	
	if(desc->flags & DDLISTENERF_MOV)
	{
		for(i=0; i<3; i++) temp[i] = FIX2FLT(desc->mov[i]);
		IDirectSound3DListener_SetVelocity(dsListener, temp[VX], temp[VY], temp[VZ], DS3D_DEFERRED);
	}
	if(desc->flags & DDLISTENERF_YAW || desc->flags & DDLISTENERF_PITCH)
	{
		float y, p;
		float top[3];	// Top vectors.
		if(desc->flags & DDLISTENERF_YAW) listenerYaw = desc->yaw;		
		if(desc->flags & DDLISTENERF_PITCH) listenerPitch = desc->pitch;
		// The angles in radians.
		y = listenerYaw / 180 * PI;
		p = (listenerPitch-90) / 180 * PI;
		// Calculate the front vector.
		temp[VX] = sin(y) * cos(p);
		temp[VZ] = cos(y) * cos(p);
		temp[VY] = sin(p);
		/*temp[VX] = sin(y);
		temp[VZ] = cos(y);
		temp[VY] = 0;*/
		// And the top vector.
		top[VX] = -sin(y) * sin(p);
		top[VZ] = -cos(y) * sin(p);
		top[VY] = cos(p);
		/*top[VX] = 0;
		top[VY] = 1;
		top[VZ] = 0;*/
		IDirectSound3DListener_SetOrientation(dsListener, 
			temp[VX], temp[VY], temp[VZ],	// Front vector.
			top[VX], top[VY], top[VZ], DS3D_DEFERRED);
	}
	if(desc->flags & DDLISTENERF_SET_REVERB && eaxListener)
	{
		val = desc->reverb.space;
		if(desc->reverb.decay > .5)
		{
			// This much decay needs at least the Generic environment.
			if(val < .2) val = .2f;
		}
		EAX_dwSet(DSPROPERTY_EAXLISTENER_ENVIRONMENT,
			val >= 1? EAX_ENVIRONMENT_PLAIN
			: val >= .8? EAX_ENVIRONMENT_CONCERTHALL
			: val >= .6? EAX_ENVIRONMENT_AUDITORIUM
			: val >= .4? EAX_ENVIRONMENT_CAVE
			: val >= .2? EAX_ENVIRONMENT_GENERIC
			: EAX_ENVIRONMENT_ROOM);

		EAX_dwSet(DSPROPERTY_EAXLISTENER_ROOM, LinLog(desc->reverb.volume));

		val = (desc->reverb.decay - .5)*1.5 + 1;
		EAX_fMul(DSPROPERTY_EAXLISTENER_DECAYTIME, val, EAXLISTENER_MINDECAYTIME,
			EAXLISTENER_MAXDECAYTIME);

		val = 1.1 * (1.2 - desc->reverb.damping);
		if(val < .1) val = .1f;
		EAX_dwMul(DSPROPERTY_EAXLISTENER_ROOMHF, val);

		EAX_fSet(DSPROPERTY_EAXLISTENER_ROOMROLLOFFFACTOR, 1.3f);

		EAX_CommitDeferred();
	}
	if(desc->flags & DDLISTENERF_DISABLE_REVERB && eaxListener)
	{
		// Turn off all reverb by setting the room value to -100 dB.
		EAX_dwSet(DSPROPERTY_EAXLISTENER_ROOM, EAXLISTENER_MINROOM);		
		EAX_CommitDeferred();
	}
}

int I2_IsSourcePlaying(sndsource_t *buf)
{
	DWORD status;
	IDirectSoundBuffer_GetStatus(buf->source, &status);
	// Restore the buffer if it's lost, but that shouldn't really matter since
	// it'll be released when it stops playing.
	if(status & DSBSTATUS_BUFFERLOST) IDirectSoundBuffer_Restore(buf->source);
	return (status & DSBSTATUS_PLAYING) != 0;
}

int I_SoundIsPlaying(int handle)
{
	sndsource_t *src = GetSource(handle);

	if(!initOk || !src) return false;
	if(src->source == NULL) return false;
	return I2_IsSourcePlaying(src);
}

// Returns a free buffer.
sndsource_t *I2_GetFreeSource(boolean play3d)
{
	int				i;
	unsigned int	num3D = 0;
	sndsource_t		*suitable = NULL, *oldest = NULL;

	if(numSources) oldest = sources;
	// We are not going to have software 3D sounds, of all things.
	// Release stopped sources and count the number of playing 3D sources.
	for(i=0; i<numSources; i++)
	{
		if(!sources[i].source) 
		{
			if(!suitable) suitable = sources + i;
			continue;
		}
		// See if this is the oldest buffer.
		if(sources[i].startTime < oldest->startTime)
		{
			// This buffer will be stopped if need be.
			oldest = sources + i;
		}
		if(I2_IsSourcePlaying(sources + i))
		{
			if(sources[i].source3D) num3D++;						
		}
		else
		{
			I2_KillSource(sources + i); // All stopped sources will be killed on sight.
			// This buffer is not playing.
			if(!suitable) suitable = sources + i;
		}
	}
	if(play3d && num3D >= dsCaps.dwMaxHw3DAllBuffers && oldest)
	{
		// There are as many 3D sources as there can be.
		// Stop the oldest sound.		
		I2_KillSource(oldest);
		return oldest;		
	}
	if(suitable) return suitable;

	// Ah well. We need to allocate a new one.
	sources = realloc(sources, sizeof(sndsource_t) * ++numSources);
	// Clear it.
	memset(sources + numSources-1, 0, sizeof(sndsource_t));
	// Return a pointer to it.
	return sources + numSources-1;
}

// Vol is linear, from 0 to 1.
void I2_SetVolume(sndsource_t *src, float vol)
{
	extern int snd_SfxVolume;
	int	ds_vol;

	vol *= snd_SfxVolume / 255.0f;
	if(vol <= 0)
		ds_vol = DSBVOLUME_MIN;
	else if(vol >= 1)
		ds_vol = DSBVOLUME_MAX;
	else 	// Straighten the volume curve.
	{
		ds_vol = 100 * 20 * log10(vol);
		if(ds_vol < DSBVOLUME_MIN) ds_vol = DSBVOLUME_MIN;
	}
	IDirectSoundBuffer_SetVolume(src->source, ds_vol);
}

void I2_SetPitch(sndsource_t *src, float pitch)
{
	int	newfreq = src->freq * pitch;
	if(newfreq < DSBFREQUENCY_MIN) newfreq = DSBFREQUENCY_MIN;
	if(newfreq > DSBFREQUENCY_MAX) newfreq = DSBFREQUENCY_MAX;
	IDirectSoundBuffer_SetFrequency(src->source, newfreq);
}

// Pan is linear, from 0 to 1. 0.5 is in the center.
void I2_SetPan(sndsource_t *src, float pan)
{
	int	ds_pan = 0;

	if(pan < 0) pan = 0;
	if(pan > 1) pan = 1;

	pan *= 2;
	pan -= 1;
	if(pan >= 1)
		ds_pan = DSBPAN_RIGHT;
	else if(pan <= -1)
		ds_pan = DSBPAN_LEFT;
	else if(pan == 0)
		ds_pan = 0;
	else if(pan > 0)
		ds_pan = -100 * 20 * log10(1-pan);// - log10(1-pan));
	else 
		ds_pan = 100 * 20 * log10(1+pan);
	IDirectSoundBuffer_SetPan(src->source, ds_pan);
}

void I2_UpdateSource(sndsource_t *buf, sound3d_t *desc)
{
	int		i;
	float	temp[3];

	if(desc->flags & DDSOUNDF_VOLUME)
	{
		I2_SetVolume(buf, desc->volume/1000.0f);
	}
	if(desc->flags & DDSOUNDF_PITCH)
	{
		I2_SetPitch(buf, desc->pitch/1000.0f);
	}
	if(desc->flags & DDSOUNDF_POS)
	{
		for(i=0; i<3; i++) temp[i] = FIX2FLT(desc->pos[i]);
		IDirectSound3DBuffer_SetPosition(buf->source3D, temp[VX], temp[VY], temp[VZ], DS3D_DEFERRED);
	}
	if(desc->flags & DDSOUNDF_MOV)
	{
		for(i=0; i<3; i++) temp[i] = FIX2FLT(desc->mov[i]);
		IDirectSound3DBuffer_SetVelocity(buf->source3D, temp[VX], temp[VY], temp[VZ], DS3D_DEFERRED);
	}
}

int CreateDSBuffer(DWORD flags, int samples, int freq, int bits, int channels, 
				   LPDIRECTSOUNDBUFFER *bufAddr)
{
	DSBUFFERDESC	bufd;
	WAVEFORMATEX	form;
	DWORD			dataBytes = samples * bits/8 * channels;
	
	// Prepare the buffer description.
	memset(&bufd, 0, sizeof(bufd));
	bufd.dwSize = sizeof(bufd);
	bufd.dwFlags = flags;
	bufd.dwBufferBytes = dataBytes;
	bufd.lpwfxFormat = &form;

	// Prepare the format description.
	memset(&form, 0, sizeof(form));
	form.wFormatTag = WAVE_FORMAT_PCM;
	form.nChannels = channels;
	form.nSamplesPerSec = freq;
	form.nBlockAlign = channels * bits/8;
	form.nAvgBytesPerSec = form.nSamplesPerSec * form.nBlockAlign;
	form.wBitsPerSample = bits;

	return IDirectSound_CreateSoundBuffer(dsound, &bufd, bufAddr, NULL);
}

int I2_PlaySound(void *data, boolean play3d, sound3d_t *desc, int pan)
{
	sampleheader_t	*header = data;
	byte			*sample = (byte*) data + sizeof(sampleheader_t);
	sndsource_t		*sndbuf;
	void			*writePtr1=NULL, *writePtr2=NULL;
	DWORD			writeBytes1, writeBytes2;
	int				samplelen = header->length, freq = header->frequency, bits = 8;

	// Can we play sounds?
	if(!initOk || data == NULL) return 0;	// Sorry...

	if(snd_Resample != 1 && snd_Resample != 2 && snd_Resample != 4)
	{
		ST_Message("I2_PlaySound: invalid resample factor.\n");
		snd_Resample = 1;
	}

	// Get a buffer that's doing nothing.
	sndbuf = I2_GetFreeSource(play3d);
	sndbuf->startTime = I_GetTime();
	// Prepare the audio data.
	if(snd_Resample == 1 && !snd_16bits) // Play sounds as normal?
	{
		// No resampling necessary.
		sndbuf->freq = header->frequency;
	}
	else
	{
		// Resample the sound.
		sample = I_Resample8bitSound(sample, header->length, freq, 
			snd_Resample, snd_16bits, &samplelen);
		if(snd_16bits) bits = 16;
		freq *= snd_Resample;
		sndbuf->freq = freq;
	}
	if(sndbuf->source)
	{
		// Release the old source.
		I2_KillSource(sndbuf);
	}
	// Create a new source.
	if(FAILED(hr = CreateDSBuffer(play3d? DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY 
		| DSBCAPS_CTRL3D | DSBCAPS_MUTE3DATMAXDISTANCE | DSBCAPS_STATIC 
		: DSBCAPS_CTRLPAN | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY 
		| DSBCAPS_STATIC, 
		header->length * snd_Resample, freq, bits, 1, &sndbuf->source)))
	{
		ST_Message("I2_PlaySound: couldn't create a new buffer (result = %d).\n", hr & 0xffff);
		memset(sndbuf, 0, sizeof(*sndbuf));
		return 0;
	}
	if(play3d)
	{
		// Query the 3D interface.
		if(FAILED(hr = IDirectSoundBuffer_QueryInterface(sndbuf->source, &IID_IDirectSound3DBuffer, 
			&sndbuf->source3D)))
		{
			ST_Message("I2_PlaySound: couldn't get 3D buffer interface (result = %d).\n", hr & 0xffff);
			IDirectSoundBuffer_Release(sndbuf->source);
			memset(sndbuf, 0, sizeof(*sndbuf));
			return 0;
		}
	}

	// Lock the buffer.
	if(FAILED(hr = IDirectSoundBuffer_Lock(sndbuf->source, 0, samplelen, 
		&writePtr1, &writeBytes1, &writePtr2, &writeBytes2, 0)))
	{
		I_Error("I2_PlaySound: couldn't lock source (result = %d).\n", hr & 0xffff);
	}

	// Copy the data over.
	memcpy(writePtr1, sample, writeBytes1);
	if(writePtr2) memcpy(writePtr2, (char*) sample + writeBytes1, writeBytes2);

	// Unlock the buffer.
	if(FAILED(hr = IDirectSoundBuffer_Unlock(sndbuf->source, writePtr1, writeBytes1, 
		writePtr2, writeBytes2)))
	{
		I_Error("I2_PlaySound: couldn't unlock source (result = %d).\n", hr & 0xffff);
		return 0;
	}

	if(play3d)
	{
		// Set the 3D parameters of the source.
		if(desc->flags & DDSOUNDF_VERY_LOUD)
		{
			// You can hear this from very far away (e.g. thunderclap).
			IDirectSound3DBuffer_SetMinDistance(sndbuf->source3D, 10000, DS3D_DEFERRED);
			IDirectSound3DBuffer_SetMaxDistance(sndbuf->source3D, 20000, DS3D_DEFERRED);	
		}
		else
		{
			IDirectSound3DBuffer_SetMinDistance(sndbuf->source3D, 100, DS3D_DEFERRED);
			IDirectSound3DBuffer_SetMaxDistance(sndbuf->source3D, MAX_SND_DIST, DS3D_DEFERRED);
		}
		if(desc->flags & DDSOUNDF_LOCAL)
			IDirectSound3DBuffer_SetMode(sndbuf->source3D, DS3DMODE_DISABLE, DS3D_DEFERRED);
	}
	else
	{
		// If playing in 2D mode, set the pan.
		I2_SetPan(sndbuf, pan/1000.0f);
	}
	I2_UpdateSource(sndbuf, desc);
	// Start playing the buffer.
	if(FAILED(hr = IDirectSoundBuffer_Play(sndbuf->source, 0, 0, 0)))
	{
		I_Error("I2_PlaySound: couldn't start source (result = %d).\n", hr & 0xffff);
		return 0;
	}
	// Return the handle to the sound source.
	return (sndbuf->id = idGen++);
}

void I2_DestroyAllSources()
{
	int		i;

	for(i=0; i<numSources; i++) I2_KillSource(sources + i);
	free(sources);
	sources = NULL;
	numSources = 0;
}

void I_Update2DSound(int handle, int volume, int pan, int pitch)
{
	sndsource_t *buf = GetSource(handle);

	if(!initOk || !buf) return;
	if(buf->source == NULL) return;
	if(buf->source3D) return;
	if(!I2_IsSourcePlaying(buf)) IDirectSoundBuffer_Play(buf->source, 0, 0, 0);
	I2_SetVolume(buf, volume/1000.0f);
	I2_SetPan(buf, pan/1000.0f);
	I2_SetPitch(buf, pitch/1000.0f);
}

void I_Update3DSound(int handle, sound3d_t *desc)
{
	sndsource_t *buf = GetSource(handle);

	if(!initOk || !buf) return;
	if(buf->source == NULL) return;
	if(buf->source3D == NULL) return;
	if(!I2_IsSourcePlaying(buf)) IDirectSoundBuffer_Play(buf->source, 0, 0, 0);
	I2_UpdateSource(buf, desc);
}


#endif