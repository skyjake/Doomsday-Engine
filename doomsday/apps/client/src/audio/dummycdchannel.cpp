/** @file dummycdchannel.cpp  Dummy audio::Channel for simulating CD playback.
 *
 * @authors Copyright © 2003-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "audio/dummycdchannel.h"

#include <de/Error>

using namespace de;

namespace audio {

DummyCdChannel::DummyCdChannel() : CdChannel()
{}

PlayingMode DummyCdChannel::mode() const
{
    return _mode;
}

void DummyCdChannel::play(PlayingMode mode)
{
    if(isPlaying()) return;
    if(mode == NotPlaying) return;

    if(_track < 0) Error("DummyCdChannel::play", "No track is bound");
    _mode = mode;
}

void DummyCdChannel::stop()
{}

bool DummyCdChannel::isPaused() const
{
    return _paused;
}

void DummyCdChannel::pause()
{
    _paused = true;
}

void DummyCdChannel::resume()
{
    _paused = false;
}

Channel &DummyCdChannel::setFrequency(de::dfloat newFrequency)
{
    _frequency = newFrequency;
    return *this;
}

Channel &DummyCdChannel::setPositioning(Positioning)
{
    // Not supported.
    return *this;
}

Channel &DummyCdChannel::setVolume(de::dfloat newVolume)
{
    _volume = newVolume;
    return *this;
}

dfloat DummyCdChannel::frequency() const
{
    return _frequency;
}

Positioning DummyCdChannel::positioning() const
{
    return StereoPositioning;  // Always.
}

dfloat DummyCdChannel::volume() const
{
    return _volume;
}

void DummyCdChannel::bindTrack(dint track)
{
    _track = de::max(-1, track);
}

}  // namespace audio
