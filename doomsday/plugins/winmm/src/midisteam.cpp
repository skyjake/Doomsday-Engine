/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * sys_musd_win.c: Music Driver for Win32 Multimedia (winmm).
 *
 * Plays MIDI streams.
 */

// HEADER FILES ------------------------------------------------------------

#include <malloc.h>
#include <math.h>
#include <stdarg.h>

#include "dswinmm.h"

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

static int          openStream(void);
static void         freeSongBuffer(void);
static void         closeStream(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static void* song;
static size_t songSize;

static MIDIHDR midiBuffers[MAX_BUFFERS];
static LPMIDIHDR loopBuffer;
static int registered;
static byte* readPos;
static int readTime; // In ticks.

static int midiAvail = false;
static UINT devId;
static HMIDISTRM midiStr;
static int playing = 0; // The song is playing/looping.
static byte chanVols[16]; // Last volume for each channel.
static int volumeShift;

static char ctrlMus2Midi[NUM_MUS_CTRLS] = {
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
 * @return              @c true, if successful.
 */
int DM_Music_Init(void)
{
    int                 i;

    if(midiAvail)
        return true; // Already initialized.

    volumeShift = ArgExists("-mdvol") ? 1 : 0; // Double music volume.

    Con_Message("DM_WinMusInit: %i MIDI-Out devices present.\n",
                midiOutGetNumDevs());

    // Open the midi stream.
    if(!openStream())
        return false;

    // Now the MIDI is available.
    Con_Message("DM_WinMusInit: MIDI initialized.\n");

    playing = false;
    registered = FALSE;
    // Clear the MIDI buffers.
    memset(midiBuffers, 0, sizeof(midiBuffers));
    // Init channel volumes.
    for(i = 0; i < 16; ++i)
        chanVols[i] = 64;

    return midiAvail = true;
}

void DM_Music_Shutdown(void)
{
    if(!midiAvail)
        return;

    midiAvail = false;
    playing = false;

    freeSongBuffer();

    closeStream();
}

void DM_Music_Update(void)
{
    // No need to do anything. The callback handles restarting.
}

static void initSongReader(musheader_t* musHdr)
{
    readPos = (byte *) musHdr + musHdr->scoreStart;
    readTime = 0;
}

/**
 * Reads the MUS data and produces the next corresponding MIDI event.
 *
 * @return              @c false, when the score ends.
 */
static int getNextEvent(MIDIEVENT* mev)
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
Con_Message("time: %i note: p1:%i p2:%i\n", mev->dwDeltaTime, midiParm1,
            midiParm2);
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
Con_Message("pitch wheel: ch %d (%x %x = %x)\n", evDesc->channel,
            midiParm1, midiParm2, midiParm1 | (midiParm2 << 7));
#endif*/
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
    if(midiChan == 15)
        midiChan = 9;
    else if(midiChan == 9)
        midiChan = 15;

/*#if _DEBUG
Con_Message("MIDI event/%d: %x %d %d\n",evDesc->channel,midiStatus,
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

static LPMIDIHDR getFreeBuffer(void)
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
            mh->lpData = malloc(mh->dwBufferLength);
            mh->dwBytesRecorded = 0;
            mh->dwFlags = 0;

            return mh;
        }
    }

    return NULL;
}

/**
 * Note that lpData changes during reallocation!
 *
 * @return              @c false, if the allocation can't be done.
 */
static int resizeWorkBuffer(LPMIDIHDR mh)
{
    // Don't allocate too large buffers.
    if(mh->dwBufferLength + BUFFER_ALLOC > MAX_BUFFER_LEN)
        return FALSE;

    mh->dwBufferLength += BUFFER_ALLOC;
    mh->lpData = realloc(mh->lpData, mh->dwBufferLength);

    // Allocation was successful.
    return TRUE;
}

/**
 * The buffer is ready, prepare it and stream out.
 */
static void beginStream(LPMIDIHDR mh)
{
    midiStreamOut(midiStr, mh, sizeof(*mh));
}

void CALLBACK DM_Music_Callback(HMIDIOUT hmo, UINT uMsg, DWORD dwInstance,
                                DWORD dwParam1, DWORD dwParam2)
{
    LPMIDIHDR           mh;

    switch(uMsg)
    {
    case MOM_DONE:
        if(!playing)
            return;

        mh = (LPMIDIHDR) dwParam1;
        // This buffer has stopped. Is this the last buffer?
        // If so, check for looping.
        if(mh == loopBuffer)
        {
            // Play all buffers again.
            DM_Music_Play(true);
        }
        break;

    default:
        break;
    }
}

static void prepareBuffers(musheader_t* song)
{
    LPMIDIHDR           mh = getFreeBuffer();
    MIDIEVENT           mev;
    DWORD*              ptr;

    if(!song)
        return;

    // First add the tempo.
    ptr = (DWORD *) mh->lpData;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = (MEVT_TEMPO << 24) | 1000000; // One second.
    mh->dwBytesRecorded = 3 * sizeof(DWORD);

    // Start reading the events.
    initSongReader(song);
    while(getNextEvent(&mev))
    {
        // Is the buffer getting full?
        if(mh->dwBufferLength - mh->dwBytesRecorded < 3 * sizeof(DWORD))
        {
            // Try to get more buffer.
            if(!resizeWorkBuffer(mh))
            {
                // Not possible, buffer size has reached the limit.
                // We need to start working on another one.
                midiOutPrepareHeader((HMIDIOUT) midiStr, mh, sizeof(*mh));
                mh = getFreeBuffer();
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

static void releaseBuffers(void)
{
    int                 i;

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
}

static void deregisterSong(void)
{
    if(!midiAvail || !registered)
        return;

    // First stop the song.
    DM_Music_Stop();

    registered = FALSE;

    // This is the actual unregistration.
    releaseBuffers();
}

/**
 * The song is already loaded in the song buffer.
 */
static int registerSong(void)
{
    if(!midiAvail)
        return false;

    deregisterSong();
    prepareBuffers(song);

    // Now there is a registered song.
    registered = TRUE;
    return true;
}

void DM_Music_Reset(void)
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

void DM_Music_Stop(void)
{
    if(!midiAvail || !playing)
        return;

    playing = false;
    loopBuffer = NULL;

    DM_Music_Reset();
}

int DM_Music_Play(int looped)
{
    int                 i;

    if(!midiAvail)
        return false;

    // Do we need to prepare the MIDI data?
    if(!registered)
        registerSong();

    playing = true;
    DM_Music_Reset();

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
    return true;
}

void DM_Music_Pause(int setPause)
{
    playing = !setPause;
    if(setPause)
        midiStreamPause(midiStr);
    else
        midiStreamRestart(midiStr);
}

void DM_Music_Set(int prop, float value)
{
    // No unique properties.
}

int DM_Music_Get(int prop, void* ptr)
{
    if(!midiAvail)
        return false;

    switch(prop)
    {
    case MUSIP_ID:
        if(ptr)
        {
            strcpy(ptr, "Win/Mus");
            return true;
        }
        break;

    default:
        break;
    }

    return false;
}

static int openStream(void)
{
    MMRESULT            mmres;
    MIDIPROPTIMEDIV     tdiv;

    devId = MIDI_MAPPER;
    if((mmres =
        midiStreamOpen(&midiStr, &devId, 1, (DWORD_PTR) DM_Music_Callback, 0,
                       CALLBACK_FUNCTION)) != MMSYSERR_NOERROR)
    {
        Con_Message("DM_WinMusOpenStream: midiStreamOpen error %i.\n", mmres);
        return FALSE;
    }

    // Set stream time format, 140 ticks per quarter note.
    tdiv.cbStruct = sizeof(tdiv);
    tdiv.dwTimeDiv = 140;
    if((mmres =
        midiStreamProperty(midiStr, (BYTE *) & tdiv,
                           MIDIPROP_SET | MIDIPROP_TIMEDIV)) !=
       MMSYSERR_NOERROR)
    {
        Con_Message("DM_WinMusOpenStream: time format! %i\n", mmres);
        return FALSE;
    }

    return TRUE;
}

static void closeStream(void)
{
    DM_Music_Reset();
    midiStreamClose(midiStr);
}

static void freeSongBuffer(void)
{
    deregisterSong();

    if(song)
        free(song);
    song = NULL;
    songSize = 0;
}

void* DM_Music_SongBuffer(size_t length)
{
    freeSongBuffer();
    songSize = length;

    return song = malloc(length);
}
