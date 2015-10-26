/** @file midistream.cpp  Plays MIDI streams via the winmm API.
 *
 * @todo Consolidate MUS -> MIDI conversion using Doomsday's own functionality.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#define WIN32_LEAN_AND_MEAN

#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0400
# undef _WIN32_WINNT
#endif
#ifndef _WIN32_WINNT
# define _WIN32_WINNT 0x0400
#endif

#include "midistream.h"

#include <de/String>
#include <windows.h>
#include <mmsystem.h>
#include <cstdio>
#include <memory>

using namespace de;

typedef unsigned char byte;

#define MAX_BUFFER_LEN      ( 65535 )
#define MAX_BUFFERS         ( 8 )
#define BUFFER_ALLOC        ( 4096 )  ///< Allocate in 4K chunks.

struct musheader_t
{
    char id[4];              ///< Identifier ("MUS" 0x1A).
    WORD scoreLen;
    WORD scoreStart;
    WORD channels;           ///< Number of primary channels.
    WORD secondaryChannels;  ///< Number of secondary channels.
    WORD instrCnt;
    WORD dummy;
    // The instrument list begins here.
};

struct museventdesc_t
{
    unsigned char channel:4;
    unsigned char event:3;
    unsigned char last:1;
};

/**
 * MUS event types.
 */
enum
{
    MUS_EV_RELEASE_NOTE,
    MUS_EV_PLAY_NOTE,
    MUS_EV_PITCH_WHEEL,
    MUS_EV_SYSTEM,        ///< Valueless controller.
    MUS_EV_CONTROLLER,
    MUS_EV_FIVE,
    MUS_EV_SCORE_END,
    MUS_EV_SEVEN
};

/**
 * MUS controllers.
 */
enum
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

static MMRESULT res;

static char const ctrlMus2Midi[NUM_MUS_CTRLS] =
{
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

DENG2_PIMPL(MidiStreamer)
{
    dint volumeShift;

    HMIDISTRM midiStr;
    UINT devId;
    dint playing;         ///< The song is playing/looping.
    bool paused = false;
    byte chanVols[16];    ///< Last volume for each channel.
    void *song;
    size_t songSize;

    MIDIHDR midiBuffers[8];
    LPMIDIHDR loopBuffer;
    dint registered;
    byte *readPos;
    dint readTime;      ///< In ticks.

    Instance(Public *i) : Base(i) {}

    static void CALLBACK Callback(HMIDIOUT /*hmo*/, UINT uMsg, DWORD_PTR dwInstance,
        DWORD dwParam1, DWORD /*dwParam2*/)
    {
        auto &inst = *(Instance *)dwInstance;

        switch(uMsg)
        {
        case MOM_DONE:
            if(inst.playing)
            {
                // This buffer has stopped. Is this the last buffer?
                // If so, check for looping.
                if((LPMIDIHDR) dwParam1 == inst.loopBuffer)
                {
                    // Play all buffers again.
                    inst.self.play(true);
                }
                else
                {
                    inst.playing = false;
                }
            }
            break;

        default: break;
        }
    }

    void deregisterSong()
    {
        if(!registered) return;

        // First stop the song.
        self.stop();

        // This is the actual unregistration.
        for(dint i = 0; i < MAX_BUFFERS; ++i)
        {
            MIDIHDR &buf = midiBuffers[i];
            if(buf.dwUser)
            {
                midiOutUnprepareHeader((HMIDIOUT) midiStr, &buf, sizeof(buf));
                free(buf.lpData);

                // Clear for re-use.
                de::zap(buf);
            }
        }

        registered = FALSE;
    }

    LPMIDIHDR getFreeBuffer()
    {
        for(dint i = 0; i < MAX_BUFFERS; ++i)
        {
            MIDIHDR &buf = midiBuffers[i];

            if(buf.dwUser == FALSE)
            {
                // Mark the header used.
                buf.dwUser = TRUE;

                // Allocate some memory for buffer.
                buf.dwBufferLength  = BUFFER_ALLOC;
                buf.lpData          = (LPSTR) malloc(buf.dwBufferLength);
                buf.dwBytesRecorded = 0;
                buf.dwFlags         = 0;

                return &buf;
            }
        }

        return nullptr;
    }

    /**
     * Note that lpData changes during reallocation!
     *
     * @return  @c false if the allocation can't be done.
     */
    dint resizeWorkBuffer(LPMIDIHDR mh)
    {
        DENG2_ASSERT(mh);

        // Don't allocate too large buffers.
        if(mh->dwBufferLength + BUFFER_ALLOC > MAX_BUFFER_LEN)
            return FALSE;

        mh->dwBufferLength += BUFFER_ALLOC;
        mh->lpData = (LPSTR) realloc(mh->lpData, mh->dwBufferLength);

        // Allocation was successful.
        return TRUE;
    }

    /**
     * Reads the MUS data and produces the next corresponding MIDI event.
     *
     * @return  @c false, when the score ends.
     */
    dint getNextEvent(MIDIEVENT *mev)
    {
        dint i;

        mev->dwDeltaTime = readTime;
        readTime = 0;

        auto *evDesc = (museventdesc_t *) readPos++;
        byte midiStatus = 0;
        byte midiChan   = 0;
        byte midiParm1  = 0;
        byte midiParm2  = 0;

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

            /*LOGDEV_AUDIO_XVERBOSE("time: %i note: p1:%i p2:%i")
                << mev->dwDeltaTime << midiParm1 << midiParm2;*/
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

            /*LOGDEV_AUDIO_XVERBOSE("pitch wheel: ch %d (%x %x = %x)")
                << evDesc->channel << midiParm1 << midiParm2 << midiParm1 | (midiParm2 << 7);*/
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

        /*LOGDEV_AUDIO_XVERBOSE("MIDI event/%d: %x %d %d")
            << evDesc->channel << midiStatus << midiParm1 << midiParm2;*/

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
};

MidiStreamer::MidiStreamer() : d(new Instance(this))
{
    d->midiStr    = nullptr;
    d->playing    = 0;
    d->registered = FALSE;
    d->song       = nullptr;

    // Clear the MIDI buffers.
    std::memset(d->midiBuffers, 0, sizeof(d->midiBuffers));

    // Init channel volumes.
    for(dint i = 0; i < 16; ++i)
    {
        d->chanVols[i] = 64;
    }
}

MidiStreamer::~MidiStreamer()
{
    closeStream();
}

void MidiStreamer::setVolumeShift(dint newVolumeShift)
{
    d->volumeShift = newVolumeShift;
}

dint MidiStreamer::volumeShift() const
{
    return d->volumeShift;
}

void MidiStreamer::openStream()
{
    d->devId = MIDI_MAPPER;
    if((::res = midiStreamOpen(&d->midiStr, &d->devId, 1, (DWORD_PTR) Instance::Callback,
                               (DWORD_PTR) d.get(), CALLBACK_FUNCTION))
       != MMSYSERR_NOERROR)
    {
        throw OpenError("MidiStreamer::openStream", "Failed to open. Error: " + String::number(::res));
    }

    // Set stream time format, 140 ticks per quarter note.
    MIDIPROPTIMEDIV tdiv;
    tdiv.cbStruct  = sizeof(tdiv);
    tdiv.dwTimeDiv = 140;

    if((::res = midiStreamProperty(d->midiStr, (BYTE *) &tdiv, MIDIPROP_SET | MIDIPROP_TIMEDIV))
       != MMSYSERR_NOERROR)
    {
        throw OpenError("MidiStreamer::openStream", "Failing setting time format. Error: " + String::number(::res));
    }
}

void MidiStreamer::closeStream()
{
    freeSongBuffer();

    if(d->midiStr)
    {
        reset();
        midiStreamClose(d->midiStr);
    }
}

void *MidiStreamer::songBuffer(dsize length)
{
    freeSongBuffer();
    d->songSize = length;
    d->song     = malloc(length);

    return d->song;
}

void MidiStreamer::freeSongBuffer()
{
    d->deregisterSong();

    if(d->song)
        free(d->song);
    d->song     = nullptr;
    d->songSize = 0;
}

void MidiStreamer::play(bool looped)
{
    d->paused = false;

    // Do we need to prepare the MIDI data?
    if(!d->registered)
    {
        // The song is already loaded in the song buffer.
        d->deregisterSong();

        // Prepare the buffers.
        if(d->song)
        {
            LPMIDIHDR mh = d->getFreeBuffer();
            DENG2_ASSERT(mh);

            // First add the tempo.
            auto ptr = (DWORD *) mh->lpData;
            *ptr++ = 0;
            *ptr++ = 0;
            *ptr++ = (MEVT_TEMPO << 24) | 1000000; // One second.
            mh->dwBytesRecorded = 3 * sizeof(DWORD);

            d->readPos  = (byte *) d->song + ((musheader_t *)d->song)->scoreStart;
            d->readTime = 0;

            // Start reading the events.
            MIDIEVENT mev;
            while(d->getNextEvent(&mev))
            {
                // Is the buffer getting full?
                if(mh->dwBufferLength - mh->dwBytesRecorded < 3 * sizeof(DWORD))
                {
                    // Try to get more buffer.
                    if(!d->resizeWorkBuffer(mh))
                    {
                        // Not possible, buffer size has reached the limit.
                        // We need to start working on another one.
                        midiOutPrepareHeader((HMIDIOUT) d->midiStr, mh, sizeof(*mh));
                        mh = d->getFreeBuffer();
                        if(!mh) return;  // Oops.
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
            midiOutPrepareHeader((HMIDIOUT) d->midiStr, mh, sizeof(*mh));
        }

        // Now there is a registered song.
        d->registered = TRUE;
    }

    d->playing = true;
    reset();

    // Stream out all buffers.
    for(dint i = 0; i < MAX_BUFFERS; ++i)
    {
        MIDIHDR &buf = d->midiBuffers[i];
        if(buf.dwUser)
        {
            d->loopBuffer = &buf;
            midiStreamOut(d->midiStr, &buf, sizeof(buf));
        }
    }

    // If we aren't looping, don't bother.
    if(!looped)
    {
        d->loopBuffer = nullptr;
    }

    // Start playing.
    midiStreamRestart(d->midiStr);
}

bool MidiStreamer::isPaused() const
{
    return d->paused;
}

void MidiStreamer::pause()
{
    if(!d->playing) return;

    d->paused = true;
    midiStreamPause(d->midiStr);
}

void MidiStreamer::resume()
{
    if(!d->playing) return;

    d->paused = false;
    midiStreamRestart(d->midiStr);
}

void MidiStreamer::reset()
{
    midiStreamStop(d->midiStr);

    // Reset all channel settings.
    for(dint i = 0; i <= 0xf; ++i)
    {
        midiOutShortMsg((HMIDIOUT) d->midiStr, 0xe0 | i | 64 << 16);  // Pitch bend.
    }

    midiOutReset((HMIDIOUT) d->midiStr);
    d->paused = false;
}

void MidiStreamer::stop()
{
    if(!d->playing) return;

    d->paused     = false;
    d->playing    = false;
    d->loopBuffer = nullptr;
    reset();
}

bool MidiStreamer::isPlaying()
{
    return d->playing;
}

dint MidiStreamer::deviceCount()  // static
{
    return midiOutGetNumDevs();
}
