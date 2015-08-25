/** @file cdaudio.h  WinMM CD-DA playback interface.
 *
 * @authors Copyright © 2008-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef WINMM_CDAUDIO_H
#define WINMM_CDAUDIO_H

#include <de/libcore.h>
#include <de/String>

/**
 * Compact Disc-Digital Audio (CD-DA) (a.k.a., "Redbook") playback.
 *
 * Uses the Windows API MCI interface.
 */
class CdAudio
{
public:
    /**
     * Construct a new CDAudio interface and initialize WinMM, ready for use.
     */
    CdAudio(de::String const &deviceId = "mycd");
    virtual ~CdAudio();

    bool isPlaying();

    void stop();

    void pause(bool setPause);

    bool play(int newTrack, bool looped = false);

public:
    void update();

private:
    DENG2_PRIVATE(d)
};

#endif  // WINMM_CDAUDIO_H
