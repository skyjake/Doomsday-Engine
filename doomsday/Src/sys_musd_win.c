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
 * sys_musd_win.c: Music Driver for Win32 Multimedia
 *
 * Plays CD music and MUS songs (translated to a MIDI stream).
 */

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <malloc.h>
#include <math.h>
#include <stdarg.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_audio.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define MAX_BUFFER_LEN		65535
#define MAX_BUFFERS			8
#define BUFFER_ALLOC		4096	// Allocate in 4K chunks.

// TYPES -------------------------------------------------------------------

typedef struct {
	char    ID[4];				// identifier "MUS" 0x1A
    WORD	scoreLen;
    WORD	scoreStart;
	WORD	channels;			// number of primary channels
	WORD	secondaryChannels;	// number of secondary channels
	WORD	instrCnt;
	WORD	dummy;
	// The instrument list begins here.
} MUSHeader_t;

typedef struct {
	unsigned char	channel : 4;
	unsigned char	event : 3;
	unsigned char	last : 1;
} MUSEventDesc_t;

enum // MUS event types.
{
	MUS_EV_RELEASE_NOTE,
	MUS_EV_PLAY_NOTE,
	MUS_EV_PITCH_WHEEL,
	MUS_EV_SYSTEM,			// Valueless controller.
	MUS_EV_CONTROLLER,		
	MUS_EV_FIVE,			// ?
	MUS_EV_SCORE_END,
	MUS_EV_SEVEN			// ?
};

enum // MUS controllers.
{
	MUS_CTRL_INSTRUMENT,
	MUS_CTRL_BANK,
	MUS_CTRL_MODULATION,
	MUS_CTRL_VOLUME,
	MUS_CTRL_PAN,
	MUS_CTRL_EXPRESSION,
	MUS_CTRL_REVERB,
	MUS_CTRL_CHORUS,
	MUS_CTRL_SUSTAIN_PEDAL,
	MUS_CTRL_SOFT_PEDAL,

	// The valueless controllers.
	MUS_CTRL_SOUNDS_OFF,
	MUS_CTRL_NOTES_OFF,
	MUS_CTRL_MONO,
	MUS_CTRL_POLY,
	MUS_CTRL_RESET_ALL,
	NUM_MUS_CTRLS
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

int		DM_WinInit(void);
void	DM_WinShutdown(void);

		// Mus Interface.
int		DM_WinMusInit(void);
void	DM_WinMusShutdown(void);
void	DM_WinMusReset();
void	DM_WinMusUpdate(void);
void	DM_WinMusSet(int property, float value);
int		DM_WinMusGet(int property, void *ptr);
void	DM_WinMusPause(int pause);
void	DM_WinMusStop(void);
void *	DM_WinMusSongBuffer(int length);
int		DM_WinMusPlay(int looped);

		// CD Interface.
int		DM_WinCDInit(void);
void	DM_WinCDShutdown(void);
void	DM_WinCDUpdate(void);
void	DM_WinCDSet(int property, float value);
int		DM_WinCDGet(int property, void *ptr);
void	DM_WinCDPause(int pause);
void	DM_WinCDStop(void);
int		DM_WinCDPlay(int track, int looped);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

musdriver_t musd_win =
{
	DM_WinInit,
	DM_WinShutdown
};

musinterface_mus_t musd_win_imus =
{
	DM_WinMusInit,
	DM_WinMusUpdate,
	DM_WinMusSet,
	DM_WinMusGet,
	DM_WinMusPause,
	DM_WinMusStop,
	DM_WinMusSongBuffer,
	DM_WinMusPlay,
};

musinterface_cd_t musd_win_icd =
{
	DM_WinCDInit,
	DM_WinCDUpdate,
	DM_WinCDSet,
	DM_WinCDGet,
	DM_WinCDPause,
	DM_WinCDStop,
	DM_WinCDPlay
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static void			*song;
static int			songSize;

static MIDIHDR		midiBuffers[MAX_BUFFERS];
static MIDIHDR		*loopBuffer;
static int			registered;
static byte			*readPos;
static int			readTime;		// In ticks.

static int			midiAvail = false;
static int			volumeShift;
static UINT			devId;
static HMIDISTRM	midiStr;
static int			origVol;		// The original MIDI volume.
static int			playing = 0;	// The song is playing/looping.
static byte			chanVols[16];	// Last volume for each channel.

static char ctrlMus2Midi[NUM_MUS_CTRLS] =
{
	0,		// Not used.
	0,		// Bank select.
	1,		// Modulation.
	7,		// Volume.
	10,		// Pan.
	11,		// Expression.
	91,		// Reverb.
	93,		// Chorus.
	64,		// Sustain pedal.
	67,		// Soft pedal.

// The valueless controllers:
	120,	// All sounds off.
	123,	// All notes off.
	126,	// Mono.
	127,	// Poly.
	121		// Reset all controllers.
};

static int			cdAvail = false, cdPlayTrack = 0;
static int			cdOrigVolume;
static boolean		cdLooping;
static double		cdStartTime, cdPauseTime, cdTrackLength;

// CODE --------------------------------------------------------------------

//===========================================================================
// DM_WinInit
//===========================================================================
int	DM_WinInit(void)
{
	volumeShift = ArgExists("-mdvol")? 1 : 0; // Double music volume.
	return true;
}

//===========================================================================
// DM_WinShutdown
//===========================================================================
void DM_WinShutdown(void)
{
	DM_WinMusShutdown();
}

//===========================================================================
// DM_WinMusInitSongReader
//===========================================================================
void DM_WinMusInitSongReader(MUSHeader_t *musHdr)
{
	readPos = (byte*) musHdr + musHdr->scoreStart;
	readTime = 0;
}

//===========================================================================
// DM_WinMusGetNextEvent
//	Returns false when the score ends. Reads the MUS data and produces 
//	the next corresponding MIDI event.
//===========================================================================
int DM_WinMusGetNextEvent(MIDIEVENT *mev)
{
	MUSEventDesc_t	*evDesc;
	byte			midiStatus, midiChan, midiParm1, midiParm2;
	int				scoreEnd = 0, i;

	mev->dwDeltaTime = readTime;
	readTime = 0;

	evDesc = (MUSEventDesc_t*) readPos++;
	midiStatus = midiChan = midiParm1 = midiParm2 = 0;
	
	// Construct the MIDI event.
	switch(evDesc->event)
	{
	case MUS_EV_RELEASE_NOTE:
		midiStatus = 0x80;
		// Which note?
		midiParm1 = *readPos++;
		break;
		
	case MUS_EV_PLAY_NOTE:
		midiStatus = 0x90;
		// Which note?
		midiParm1 = *readPos++;
		// Is the volume there, too?
		if(midiParm1 & 0x80)	
			chanVols[evDesc->channel] = *readPos++;
		midiParm1 &= 0x7f;
		if((i = chanVols[evDesc->channel] << volumeShift) > 127) i = 127;
		midiParm2 = i; 
		//Con_Message( "time: %i note: p1:%i p2:%i\n", mev->dwDeltaTime, midiParm1, midiParm2);
		break;				
		
	case MUS_EV_CONTROLLER:
		midiStatus = 0xb0;
		midiParm1 = *readPos++;
		midiParm2 = *readPos++;
		// The instrument control is mapped to another kind of MIDI event.
		if(midiParm1 == MUS_CTRL_INSTRUMENT)
		{
			midiStatus = 0xc0;
			midiParm1 = midiParm2;
			midiParm2 = 0;
		}
		else
		{
			// Use the conversion table.
			midiParm1 = ctrlMus2Midi[midiParm1];
		}
		break;
	
	// 2 bytes, 14 bit value. 0x2000 is the center. 
	// First seven bits go to parm1, the rest to parm2.
	case MUS_EV_PITCH_WHEEL:
		midiStatus = 0xe0;
		i = *readPos++ << 6;
		midiParm1 = i & 0x7f;
		midiParm2 = i >> 7;
		//Con_Message( "pitch wheel: ch %d (%x %x = %x)\n", evDesc->channel, midiParm1, midiParm2, midiParm1 | (midiParm2 << 7));
		break;
		
	case MUS_EV_SYSTEM:
		midiStatus = 0xb0;
		midiParm1 = ctrlMus2Midi[*readPos++];
		break;
		
	case MUS_EV_SCORE_END:
		// We're done.
		return FALSE;
		
	default:
		Con_Error("MUS_SongPlayer: Unknown MUS event %d.\n", evDesc->event);
	}

	// Choose the channel.
	midiChan = evDesc->channel;
	// Redirect MUS channel 16 to MIDI channel 10 (percussion).
	if(midiChan == 15) midiChan = 9; else if(midiChan == 9) midiChan = 15;
	//Con_Message("MIDI event/%d: %x %d %d\n",evDesc->channel,midiStatus,midiParm1,midiParm2);

	mev->dwEvent = (MEVT_SHORTMSG << 24) | midiChan | midiStatus
		| (midiParm1 << 8) | (midiParm2 << 16);

	// Check if this was the last event in a group.
	if(!evDesc->last) return TRUE;

	// Read the time delta.
	for(readTime = 0;;)
	{
		midiParm1 = *readPos++;
		readTime = readTime*128 + (midiParm1 & 0x7f);
		if(!(midiParm1 & 0x80)) break;
	}
	return TRUE;
}

//===========================================================================
// DM_WinMusGetFreeBuffer
//===========================================================================
MIDIHDR *DM_WinMusGetFreeBuffer()
{
	int	i;

	for(i = 0; i < MAX_BUFFERS; i++)
		if(midiBuffers[i].dwUser == FALSE)
		{
			MIDIHDR *mh = midiBuffers + i;
			
			// Mark the header used.
			mh->dwUser = TRUE;
			
			// Allocate some memory for buffer.
			mh->dwBufferLength = BUFFER_ALLOC;
			mh->lpData = malloc(mh->dwBufferLength);
			mh->dwBytesRecorded = 0;

			mh->dwFlags = 0;
			return mh;
		}
		return NULL;
}

//===========================================================================
// DM_WinMusAllocMoreBuffer
//	Note that lpData changes during reallocation!
//	Returns false if the allocation can't be done.
//===========================================================================
int DM_WinMusAllocMoreBuffer(MIDIHDR *mh)
{
	// Don't allocate too large buffers.
	if(mh->dwBufferLength + BUFFER_ALLOC > MAX_BUFFER_LEN)
		return FALSE;

	mh->dwBufferLength += BUFFER_ALLOC;
	mh->lpData = realloc(mh->lpData, mh->dwBufferLength);

	// Allocation was successful.
	return TRUE;
}

//===========================================================================
// DM_WinMusStreamOut
//	The buffer is ready, prepare it and stream out.
//===========================================================================
void DM_WinMusStreamOut(MIDIHDR *mh)
{
	midiStreamOut(midiStr, mh, sizeof(*mh));
}

//===========================================================================
// DM_WinMusCallback
//===========================================================================
void CALLBACK DM_WinMusCallback(HMIDIOUT hmo, UINT wMsg, DWORD dwInstance, 
								DWORD dwParam1, DWORD dwParam2)
{
	MIDIHDR *mh;

	if(wMsg != MOM_DONE) return;

	if(!playing) return;

	mh = (MIDIHDR*) dwParam1;
	// This buffer has stopped. Is this the last buffer?
	// If so, check for looping.
	if(mh == loopBuffer)
	{
		// Play all buffers again.
		DM_WinMusPlay(true);
	}
}

//===========================================================================
// DM_WinMusPrepareBuffers
//===========================================================================
void DM_WinMusPrepareBuffers(MUSHeader_t *song)
{
	MIDIHDR		*mh = DM_WinMusGetFreeBuffer();
	MIDIEVENT	mev;
	DWORD		*ptr;

	// First add the tempo.
	ptr = (DWORD*) mh->lpData;
	*ptr++ = 0;
	*ptr++ = 0;
	*ptr++ = (MEVT_TEMPO << 24) | 1000000; // One second.
	mh->dwBytesRecorded = 3*sizeof(DWORD);
		
	// Start reading the events.
	DM_WinMusInitSongReader(song);
	while(DM_WinMusGetNextEvent(&mev))
	{
		// Is the buffer getting full?
		if(mh->dwBufferLength - mh->dwBytesRecorded < 3*sizeof(DWORD))
		{
			// Try to get more buffer.
			if(!DM_WinMusAllocMoreBuffer(mh))
			{
				// Not possible, buffer size has reached the limit.
				// We need to start working on another one.
				midiOutPrepareHeader( (HMIDIOUT) midiStr, mh, sizeof(*mh));
				mh = DM_WinMusGetFreeBuffer();
				if(!mh) return;	// Oops.
			}
		}
		// Add the event.
		ptr = (DWORD*) (mh->lpData + mh->dwBytesRecorded);
		*ptr++ = mev.dwDeltaTime;
		*ptr++ = 0;
		*ptr++ = mev.dwEvent;
		mh->dwBytesRecorded += 3*sizeof(DWORD);
	}
	// Prepare the last buffer, too.
	midiOutPrepareHeader( (HMIDIOUT) midiStr, mh, sizeof(*mh));
}

//===========================================================================
// DM_WinMusReleaseBuffers
//===========================================================================
void DM_WinMusReleaseBuffers(void)
{
	int	i;

	for(i = 0; i < MAX_BUFFERS; i++)
		if(midiBuffers[i].dwUser)
		{
			MIDIHDR *mh = midiBuffers + i;

			midiOutUnprepareHeader( (HMIDIOUT) midiStr, mh, sizeof(*mh));
			free(mh->lpData);
			
			// Clear for re-use.
			memset(mh, 0, sizeof(*mh));
		}
}

//==========================================================================
// DM_WinMusUnregisterSong
//==========================================================================
void DM_WinMusUnregisterSong(void)
{
	if(!midiAvail || !registered) return;

	// First stop the song.
	DM_WinMusStop();

	registered = FALSE;

	// This is the actual unregistration.
	DM_WinMusReleaseBuffers();
}

//==========================================================================
// DM_WinMusRegisterSong
//	The song is already loaded in the song buffer.
//==========================================================================
int DM_WinMusRegisterSong(void)
{
	if(!midiAvail) return false;

	DM_WinMusUnregisterSong();	
	DM_WinMusPrepareBuffers(song);

	// Now there is a registered song.
	registered = TRUE;
	return true;
}

//===========================================================================
// DM_WinMusReset
//===========================================================================
void DM_WinMusReset(void)
{
	int i;

	midiStreamStop(midiStr);
	// Reset channel settings.
	for(i = 0; i <= 0xf; i++) // All channels.
		midiOutShortMsg( (HMIDIOUT) midiStr, 0xe0 | i | 64<<16); // Pitch bend.
	midiOutReset( (HMIDIOUT) midiStr);	
}

//===========================================================================
// DM_WinMusStop
//===========================================================================
void DM_WinMusStop(void)
{
	if(!midiAvail || !playing) return;

	playing = false;
	loopBuffer = NULL;

	DM_WinMusReset();
}

//==========================================================================
// DM_WinMusPlay
//==========================================================================
int	DM_WinMusPlay(int looped)
{
	int		i;

	if(!midiAvail) return false;

	// Do we need to prepare the MIDI data?
	if(!registered) DM_WinMusRegisterSong();

	playing = true;
	DM_WinMusReset();

	// Stream out all buffers.
	for(i = 0; i < MAX_BUFFERS; i++)
		if(midiBuffers[i].dwUser)
		{
			loopBuffer = &midiBuffers[i];
			midiStreamOut(midiStr, &midiBuffers[i], sizeof(midiBuffers[i]));
		}

	// If we aren't looping, don't bother.
	if(!looped) loopBuffer = NULL;

	// Start playing.
	midiStreamRestart(midiStr);
	return true;
}

//===========================================================================
// DM_WinMusPause
//===========================================================================
void DM_WinMusPause(int setPause)
{
	playing = !setPause;
	if(setPause) 
		midiStreamPause(midiStr); 
	else 
		midiStreamRestart(midiStr);
}

//==========================================================================
// DM_WinMusSetMasterVolume
//	Vol is from 0 to 255.
//==========================================================================
void DM_WinMusSetMasterVolume(int vol)
{
	// Clamp it to a byte.
	if(vol < 0) vol = 0;
	if(vol > 255) vol = 255;

	Sys_Mixer4i(MIX_MIDI, MIX_SET, MIX_VOLUME, vol);

	// Straighten the volume curve.
	vol <<= 8;	// Make it a word.
	vol = (int) (255.9980469 * sqrt(vol));
	// Expand to a dword.
	//ret = midiOutSetVolume( (HMIDIOUT) midiStr, vol + (vol<<16));	
	

}

//===========================================================================
// DM_WinMusSet
//===========================================================================
void DM_WinMusSet(int property, float value)
{
	if(!midiAvail) return;
	switch(property)
	{
	case MUSIP_VOLUME:
		DM_WinMusSetMasterVolume(value*255 + 0.5);
		break;
	}
}

//===========================================================================
// DM_WinMusGet
//===========================================================================
int DM_WinMusGet(int property, void *ptr)
{
	if(!midiAvail) return false;
	switch(property)
	{
	case MUSIP_ID:
		strcpy(ptr, "Win/Mus");
		break;
	
	default:
		return false;
	}
	return true;
}

//===========================================================================
// DM_WinMusOpenStream
//===========================================================================
int DM_WinMusOpenStream(void)
{
	MMRESULT		mmres;
	MIDIPROPTIMEDIV	tdiv;

	devId = MIDI_MAPPER;
	if((mmres = midiStreamOpen(&midiStr, &devId, 1, 
		(DWORD) DM_WinMusCallback, 0,
		CALLBACK_FUNCTION)) != MMSYSERR_NOERROR)
	{
		Con_Message("DM_WinMusOpenStream: midiStreamOpen error %i.\n",mmres);
		return FALSE;
	}
	// Set stream time format, 140 ticks per quarter note.
	tdiv.cbStruct = sizeof(tdiv);
	tdiv.dwTimeDiv = 140;
	if((mmres = midiStreamProperty(midiStr, (BYTE*) &tdiv, 
		MIDIPROP_SET | MIDIPROP_TIMEDIV)) != MMSYSERR_NOERROR)
	{
		Con_Message("DM_WinMusOpenStream: time format! %i\n", mmres);
		return FALSE;
	}
	return TRUE;
}

//===========================================================================
// DM_WinMusCloseStream
//===========================================================================
void DM_WinMusCloseStream(void)
{
	DM_WinMusReset();
	midiStreamClose(midiStr);
}

//===========================================================================
// DM_WinMusFreeSongBuffer
//===========================================================================
void DM_WinMusFreeSongBuffer(void)
{
	DM_WinMusUnregisterSong();

	if(song) free(song);
	song = NULL;
	songSize = 0;
}

//===========================================================================
// DM_WinMusSongBuffer
//===========================================================================
void* DM_WinMusSongBuffer(int length)
{
	DM_WinMusFreeSongBuffer();
	songSize = length;
	return song = malloc(length);
}

//==========================================================================
// DM_WinMusInit
//	Returns true if successful.
//==========================================================================
int DM_WinMusInit(void)
{
	int	i;

	if(midiAvail) return true;	// Already initialized.

	Con_Message("DM_WinMusInit: %i MIDI-Out devices present.\n",
		midiOutGetNumDevs());

	// Open the midi stream.
	if(!DM_WinMusOpenStream()) return false;

	// Now the MIDI is available.
	Con_Message("DM_WinMusInit: MIDI initialized.\n");

	// Get the original MIDI volume (restored at shutdown).
	//midiOutGetVolume( (HMIDIOUT) midiStr, &origVol);
	origVol = Sys_Mixer3i(MIX_MIDI, MIX_GET, MIX_VOLUME);
	
	playing = false;
	registered = FALSE;
	// Clear the MIDI buffers.
	memset(midiBuffers, 0, sizeof(midiBuffers));
	// Init channel volumes.
	for(i = 0; i < 16; i++) chanVols[i] = 64;
	return midiAvail = true;
}

//==========================================================================
// DM_WinMusShutdown
//==========================================================================
void DM_WinMusShutdown()
{
	if(!midiAvail) return;
	midiAvail = false;
	playing = false;
	
	DM_WinMusFreeSongBuffer();

	// Restore the original volume.
	//midiOutSetVolume((HMIDIOUT) midiStr, origVol);
	Sys_Mixer4i(MIX_MIDI, MIX_SET, MIX_VOLUME, origVol);

	DM_WinMusCloseStream();
}

//===========================================================================
// DM_WinMusUpdate
//===========================================================================
void DM_WinMusUpdate(void)
{
	// No need to do anything. The callback handles restarting.
}

//===========================================================================
// DM_WinCDCommand
//	Execute an MCI command string. Returns true if successful.
//===========================================================================
int DM_WinCDCommand(char *returnInfo, int returnLength, 
					const char *format, ...)
{
	char		buf[300];
	va_list		args;
	MCIERROR	error;

	va_start(args, format);
	vsprintf(buf, format, args);
	va_end(args);

	if((error = mciSendString(buf, returnInfo, returnLength, NULL)))
	{
		mciGetErrorString(error, buf, 300);
		Con_Message("DM_WinCD: %s\n", buf);
		return false;
	}
	return true;
}

//===========================================================================
// DM_WinCDInit
//===========================================================================
int DM_WinCDInit(void)
{
	if(cdAvail) return true;

	if(!DM_WinCDCommand(0, 0, "open cdaudio alias mycd"))
		return false;

	if(!DM_WinCDCommand(0, 0, "set mycd time format tmsf"))
		return false;

	// Get the original CD volume.
	cdOrigVolume = Sys_Mixer3i(MIX_CDAUDIO, MIX_GET, MIX_VOLUME);
	
	// Successful initialization.
	cdPlayTrack = 0;
	return cdAvail = true;
}

//===========================================================================
// DM_WinCDShutdown
//===========================================================================
void DM_WinCDShutdown(void)
{
	if(!cdAvail) return;
	DM_WinCDStop();
	DM_WinCDCommand(0, 0, "close mycd");
	// Restore original CD volume, if possible.
	if(cdOrigVolume != MIX_ERROR)
		Sys_Mixer4i(MIX_CDAUDIO, MIX_SET, MIX_VOLUME, cdOrigVolume);
	cdAvail = false;
}

//===========================================================================
// DM_WinCDUpdate
//===========================================================================
void DM_WinCDUpdate(void)
{
	if(!cdAvail) return;

	// Check for looping.
	if(cdPlayTrack
		&& cdLooping
		&& Sys_GetSeconds() - cdStartTime > cdTrackLength)
	{
		// Restart the track.
		DM_WinCDPlay(cdPlayTrack, true);		
	}
}

//===========================================================================
// DM_WinCDSet
//===========================================================================
void DM_WinCDSet(int property, float value)
{
	if(!cdAvail) return;
	switch(property)
	{
	case MUSIP_VOLUME:
		Sys_Mixer4i(MIX_CDAUDIO, MIX_SET, MIX_VOLUME, value*255 + 0.5);
		break;
	}
}

//===========================================================================
// DM_WinCDGet
//===========================================================================
int DM_WinCDGet(int property, void *ptr)
{
	if(!cdAvail) return false;
	switch(property)
	{
	case MUSIP_ID:
		strcpy(ptr, "Win/CD");
		break;
	
	default:
		return false;
	}
	return true;
}

//===========================================================================
// DM_WinCDPause
//===========================================================================
void DM_WinCDPause(int pause)
{
	if(!cdAvail) return;
	DM_WinCDCommand(0, 0, "%s mycd", pause? "pause" : "play");
	if(pause) 
		cdPauseTime = Sys_GetSeconds();
	else
		cdStartTime += Sys_GetSeconds() - cdPauseTime;
}

//===========================================================================
// DM_WinCDStop
//===========================================================================
void DM_WinCDStop(void)
{
	if(!cdAvail || !cdPlayTrack) return;
	cdPlayTrack = 0;
	DM_WinCDCommand(0, 0, "stop mycd");
}

//===========================================================================
// DM_WinCDGetTrackLength
//	Returns the length of the track in seconds.
//===========================================================================
int DM_WinCDGetTrackLength(int track)
{
	char lenString[80];
	int min, sec;

	if(!DM_WinCDCommand(lenString, 80, "status mycd length track %i", track))
		return 0;

	sscanf(lenString, "%i:%i", &min, &sec);	
	return min*60 + sec;
}

//===========================================================================
// DM_WinCDPlay
//===========================================================================
int	DM_WinCDPlay(int track, int looped)
{
	int len;

	if(!cdAvail) return false;

	// Get the length of the track.
	cdTrackLength = len = DM_WinCDGetTrackLength(track);
	if(!len) return false; // Hmm?!

	// Play it!
	if(!DM_WinCDCommand(0, 0, "play mycd from %i to %i",
		track, MCI_MAKE_TMSF(track, 0, len, 0))) return false;

	// Success!
	cdLooping = looped;
	cdStartTime = Sys_GetSeconds();
	return cdPlayTrack = track;
}

