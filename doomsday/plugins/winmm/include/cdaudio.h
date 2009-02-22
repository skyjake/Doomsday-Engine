/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2008-2009 Daniel Swanson <danij@dengine.net>
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
 * cdaudio.h: Plays CDAudio tracks via winmm API.
 */

#ifndef __WIN_CDAUDIO_H__
#define __WIN_CDAUDIO_H__

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

class WinCDAudio {
public:
    WinCDAudio();
    ~WinCDAudio(void);

    void        Update(void);
    int         Play(int track, int looped);
    void        Pause(int pause);
    void        Stop(void);

private:
    int         IsMediaPresent(void);
    int         GetTrackLength(int track);

protected:
    int         isInited;

    // Currently playing track info:
    int         currentTrack;
    int         isLooping;
    double      startTime, pauseTime, trackLength;
};

#endif
