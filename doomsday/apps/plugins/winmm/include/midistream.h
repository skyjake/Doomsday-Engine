/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2008 Daniel Swanson <danij@dengine.net>
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
 * midistream.h: Plays MIDI streams via the winmm API.
 */

#ifndef __WIN_MIDISTREAMER_H__
#define __WIN_MIDISTREAMER_H__

#define WIN32_LEAN_AND_MEAN

#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0400
# undef _WIN32_WINNT
#endif
#ifndef _WIN32_WINNT
# define _WIN32_WINNT 0x0400
#endif

#include <windows.h>
#include <mmsystem.h>

typedef unsigned char   byte;

class WinMIDIStreamer {
public:
    WinMIDIStreamer(void);
    ~WinMIDIStreamer(void);
    int         OpenStream(void);
    void*       SongBuffer(size_t length);
    void        FreeSongBuffer(void);
    void        CloseStream(void);
    void        Play(int looped);
    void        Pause(int setPause);
    void        Reset(void);
    void        Stop(void);
    int         IsPlaying(void);

    int         volumeShift;

protected:
    static void CALLBACK Callback(HMIDIOUT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
    LPMIDIHDR   GetFreeBuffer(void);
    int         ResizeWorkBuffer(LPMIDIHDR mh);
    int         GetNextEvent(MIDIEVENT* mev);
    void        DeregisterSong(void);

    HMIDISTRM   midiStr;
    UINT        devId;
    int         playing; // The song is playing/looping.
    byte        chanVols[16]; // Last volume for each channel.
    void*       song;
    size_t      songSize;

    MIDIHDR     midiBuffers[8];
    LPMIDIHDR   loopBuffer;
    int         registered;
    byte*       readPos;
    int         readTime; // In ticks.
};

#endif
