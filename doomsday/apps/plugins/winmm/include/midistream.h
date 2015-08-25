/**@file midistream.h  Plays MIDI streams via the WinMM API.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef WINMM_MIDISTREAMER_H
#define WINMM_MIDISTREAMER_H

#include <de/libcore.h>
#include <de/Error>

class MidiStreamer
{
public:
    /// Failed openning the output stream. @ingroup errors
    DENG2_ERROR(OpenError);

public:
    MidiStreamer();
    virtual ~MidiStreamer();

    void setVolumeShift(de::dint newVolumeShift);
    de::dint volumeShift() const;

    void openStream();
    void closeStream();

    void *songBuffer(de::dsize length);
    void freeSongBuffer();

    bool isPlaying();
    void reset();
    void stop();

    void pause(bool setPause = true);

    void play(bool looped = false);

public:
    /**
     * Query the number of MIDI output devices on the host system.
     */
    static de::dint deviceCount();

private:
    DENG2_PRIVATE(d)
};

#endif  // WINMM_MIDISTREAMER_H
