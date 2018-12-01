/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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

/**
 * midistream.cpp: Plays MIDI streams via the winmm API.
 */

// HEADER FILES ------------------------------------------------------------

#include <malloc.h>
#include <stdio.h>

#include "midistream.h"

// MACROS ------------------------------------------------------------------

#define MAX_BUFFER_LEN      (65535)
#define MAX_BUFFERS         (8)
#define BUFFER_ALLOC        (4096) // Allocate in 4K chunks.

// TYPES -------------------------------------------------------------------

typedef struct {
    char        id[4]; // Identifier ("MUS" 0x1A).
    WORD        scoreLen;
    WORD        scoreStart;
    WORD        channels; // Number of primary channels.
    WORD        secondaryChannels; // Number of secondary channels.
    WORD        instrCnt;
    WORD        dummy;
    // The instrument list begins here.
} musheader_t;

typedef struct {
    unsigned char channel:4;
    unsigned char event:3;
    unsigned char last:1;
} museventdesc_t;

enum // MUS event types.
{
    MUS_EV_RELEASE_NOTE,
    MUS_EV_PLAY_NOTE,
    MUS_EV_PITCH_WHEEL,
    MUS_EV_SYSTEM, // Valueless controller.
    MUS_EV_CONTROLLER,
    MUS_EV_FIVE, // ?
    MUS_EV_SCORE_END,
    MUS_EV_SEVEN // ?
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

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const char ctrlMus2Midi[NUM_MUS_CTRLS] = {
    0, // Not used.
    0, // Bank select.
    1, // Modulation.
    7, // Volume.
    10, // Pan.
    11, // Expression.
    91, // Reverb.
    93, // Chorus.
    64, // Sustain pedal.
    67, // Soft pedal.

    // The valueless controllers:
    120, // All sounds off.
    123, // All notes off.
    126, // Mono.
    127, // Poly.
    121  // Reset all controllers.
};

// CODE --------------------------------------------------------------------

/**
 * WinMIDIStreamer - Constructor.
 */
WinMIDIStreamer::WinMIDIStreamer(void)
{
    UINT                i;

    midiStr = NULL;
    playing = 0;
    registered = FALSE;
    song = NULL;

    // Clear the MIDI buffers.
    memset(midiBuffers, 0, sizeof(midiBuffers));
    // Init channel volumes.
    for(i = 0; i < 16; ++i)
        chanVols[i] = 64;
}

/**
 * WinMIDIStreamer - Destructor.
 */
WinMIDIStreamer::~WinMIDIStreamer(void)
{
    CloseStream();
}

int WinMIDIStreamer::OpenStream(void)
{
    MMRESULT            mmres;
    MIDIPROPTIMEDIV     tdiv;

    devId = MIDI_MAPPER;
    if(MMSYSERR_NOERROR != (mmres =
        midiStreamOpen(&midiStr, &devId, 1, (DWORD_PTR) Callback,
                       (DWORD_PTR) this, CALLBACK_FUNCTION)))
    {
        printf("WinMIDIStreamer::OpenStream: midiStreamOpen error %i.\n", mmres);
        return FALSE;
    }

    // Set stream time format, 140 ticks per quarter note.
    tdiv.cbStruct = sizeof(tdiv);
    tdiv.dwTimeDiv = 140;
    if(MMSYSERR_NOERROR != (mmres =
        midiStreamProperty(midiStr, (BYTE *) & tdiv,
                           MIDIPROP_SET | MIDIPROP_TIMEDIV)))
    {
        printf("WinMIDIStreamer::OpenStream: time format! %i\n", mmres);
        return FALSE;
    }

    return TRUE;
}

void WinMIDIStreamer::CloseStream(void)
{
    FreeSongBuffer();

    if(midiStr)
    {
        Reset();
        midiStreamClose(midiStr);
    }
}

/**
 * Reads the MUS data and produces the next corresponding MIDI event.
 *
 * @return              @c false, when the score ends.
 */
int WinMIDIStreamer::GetNextEvent(MIDIEVENT* mev)
{
    int                 i;
    museventdesc_t*     evDesc;
    byte                midiStatus, midiChan, midiParm1, midiParm2;

    mev->dwDeltaTime = readTime;
    readTime = 0;

    evDesc = (museventdesc_t *) readPos++;
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
        if((i = chanVols[evDesc->channel] << volumeShift) > 127)
            i = 127;
        midiParm2 = i;

/*#if _DEBUG
        App_Log(DE2_DEV_AUDIO_XVERBOSE, "time: %i note: p1:%i p2:%i",
            mev->dwDeltaTime, midiParm1, midiParm2);
#endif*/
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

    case MUS_EV_PITCH_WHEEL:
        /**
         * 2 bytes, 14 bit value. 0x2000 is the center.
         * First seven bits go to parm1, the rest to parm2.
         */

        midiStatus = 0xe0;
        i = *readPos++ << 6;
        midiParm1 = i & 0x7f;
        midiParm2 = i >> 7;

/*#if _DEBUG
        App_Log(DE2_DEV_AUDIO_XVERBOSE, "pitch wheel: ch %d (%x %x = %x)", evDesc->channel,
            midiParm1, midiParm2, midiParm1 | (midiParm2 << 7));
#endif*/
        break;

    case MUS_EV_SYSTEM:
        midiStatus = 0xb0;
        midiParm1 = ctrlMus2Midi[*readPos++];
        break;

    default:
    case MUS_EV_SCORE_END:
        // We're done.
        return FALSE;
    }

    // Choose the channel.
    midiChan = evDesc->channel;
    // Redirect MUS channel 16 to MIDI channel 10 (percussion).
    if(midiChan == 15)
        midiChan = 9;
    else if(midiChan == 9)
        midiChan = 15;

/*#if _DEBUG
    App_Log(DE2_DEV_AUDIO_XVERBOSE, "MIDI event/%d: %x %d %d", evDesc->channel, midiStatus,
            midiParm1,midiParm2);
#endif*/

    mev->dwEvent =
        (MEVT_SHORTMSG << 24) | midiChan | midiStatus | (midiParm1 << 8) |
        (midiParm2 << 16);

    // Check if this was the last event in a group.
    if(!evDesc->last)
        return TRUE;

    // Read the time delta.
    readTime = 0;
    do
    {
        midiParm1 = *readPos++;
        readTime = readTime * 128 + (midiParm1 & 0x7f);
    } while(midiParm1 & 0x80);

    return TRUE;
}

/**
 * Note that lpData changes during reallocation!
 *
 * @return              @c false, if the allocation can't be done.
 */
int WinMIDIStreamer::ResizeWorkBuffer(LPMIDIHDR mh)
{
    // Don't allocate too large buffers.
    if(mh->dwBufferLength + BUFFER_ALLOC > MAX_BUFFER_LEN)
        return FALSE;

    mh->dwBufferLength += BUFFER_ALLOC;
    mh->lpData = (LPSTR) realloc(mh->lpData, mh->dwBufferLength);

    // Allocation was successful.
    return TRUE;
}

void* WinMIDIStreamer::SongBuffer(size_t length)
{
    FreeSongBuffer();
    songSize = length;

    return song = malloc(length);
}

void WinMIDIStreamer::FreeSongBuffer(void)
{
    DeregisterSong();

    if(song)
        free(song);
    song = NULL;
    songSize = 0;
}

LPMIDIHDR WinMIDIStreamer::GetFreeBuffer(void)
{
    int                 i;

    for(i = 0; i < MAX_BUFFERS; ++i)
    {
        LPMIDIHDR           mh = &midiBuffers[i];

        if(mh->dwUser == FALSE)
        {
            // Mark the header used.
            mh->dwUser = TRUE;

            // Allocate some memory for buffer.
            mh->dwBufferLength = BUFFER_ALLOC;
            mh->lpData = (LPSTR) malloc(mh->dwBufferLength);
            mh->dwBytesRecorded = 0;
            mh->dwFlags = 0;

            return mh;
        }
    }

    return NULL;
}

void WinMIDIStreamer::Play(int looped)
{
    UINT                i;

    // Do we need to prepare the MIDI data?
    if(!registered)
    {
        // The song is already loaded in the song buffer.
        DeregisterSong();

        // Prepare the buffers.
        if(song)
        {
            LPMIDIHDR           mh = GetFreeBuffer();
            MIDIEVENT           mev;
            DWORD*              ptr;

            // First add the tempo.
            ptr = (DWORD *) mh->lpData;
            *ptr++ = 0;
            *ptr++ = 0;
            *ptr++ = (MEVT_TEMPO << 24) | 1000000; // One second.
            mh->dwBytesRecorded = 3 * sizeof(DWORD);

            // Start reading the events.
            readPos = (byte *) song + ((musheader_t*)song)->scoreStart;
            readTime = 0;
            while(GetNextEvent(&mev))
            {
                // Is the buffer getting full?
                if(mh->dwBufferLength - mh->dwBytesRecorded < 3 * sizeof(DWORD))
                {
                    // Try to get more buffer.
                    if(!ResizeWorkBuffer(mh))
                    {
                        // Not possible, buffer size has reached the limit.
                        // We need to start working on another one.
                        midiOutPrepareHeader((HMIDIOUT) midiStr, mh, sizeof(*mh));
                        mh = GetFreeBuffer();
                        if(!mh)
                            return; // Oops.
                    }
                }

                // Add the event.
                ptr = (DWORD *) (mh->lpData + mh->dwBytesRecorded);
                *ptr++ = mev.dwDeltaTime;
                *ptr++ = 0;
                *ptr++ = mev.dwEvent;
                mh->dwBytesRecorded += 3 * sizeof(DWORD);
            }

            // Prepare the last buffer, too.
            midiOutPrepareHeader((HMIDIOUT) midiStr, mh, sizeof(*mh));
        }

        // Now there is a registered song.
        registered = TRUE;
    }

    playing = true;
    Reset();

    // Stream out all buffers.
    for(i = 0; i < MAX_BUFFERS; ++i)
    {
        if(midiBuffers[i].dwUser)
        {
            loopBuffer = &midiBuffers[i];
            midiStreamOut(midiStr, &midiBuffers[i], sizeof(midiBuffers[i]));
        }
    }

    // If we aren't looping, don't bother.
    if(!looped)
        loopBuffer = NULL;

    // Start playing.
    midiStreamRestart(midiStr);
}

void WinMIDIStreamer::Pause(int setPause)
{
    playing = !setPause;
    if(setPause)
        midiStreamPause(midiStr);
    else
        midiStreamRestart(midiStr);
}

void WinMIDIStreamer::Reset(void)
{
    int                 i;

    midiStreamStop(midiStr);

    // Reset channel settings.
    for(i = 0; i <= 0xf; ++i) // All channels.
    {
        midiOutShortMsg((HMIDIOUT) midiStr, 0xe0 | i | 64 << 16); // Pitch bend.
    }

    midiOutReset((HMIDIOUT) midiStr);
}

void WinMIDIStreamer::Stop(void)
{
    if(!playing)
        return;

    playing = false;
    loopBuffer = NULL;
    Reset();
}

void WinMIDIStreamer::DeregisterSong(void)
{
    int                 i;

    if(!registered)
        return;

    // First stop the song.
    Stop();

    // This is the actual unregistration.
    for(i = 0; i < MAX_BUFFERS; ++i)
    {
        if(midiBuffers[i].dwUser)
        {
            LPMIDIHDR               mh = &midiBuffers[i];

            midiOutUnprepareHeader((HMIDIOUT) midiStr, mh, sizeof(*mh));
            free(mh->lpData);

            // Clear for re-use.
            memset(mh, 0, sizeof(*mh));
        }
    }

    registered = FALSE;
}

int WinMIDIStreamer::IsPlaying(void)
{
    return playing;
}

void CALLBACK WinMIDIStreamer::Callback(HMIDIOUT /*hmo*/, UINT uMsg,
                                        DWORD_PTR dwInstance, DWORD_PTR dwParam1,
                                        DWORD_PTR /*dwParam2*/)
{
    WinMIDIStreamer*    me = (WinMIDIStreamer*) dwInstance;
    LPMIDIHDR           mh;

    switch(uMsg)
    {
    case MOM_DONE:
        if(!me->playing)
            return;

        mh = reinterpret_cast<LPMIDIHDR>(dwParam1);

        // This buffer has stopped. Is this the last buffer?
        // If so, check for looping.
        if(mh == me->loopBuffer)
        {
            // Play all buffers again.
            me->Play(true);
        }
        else
        {
            me->playing = false;
        }
        break;

    default:
        break;
    }
}
