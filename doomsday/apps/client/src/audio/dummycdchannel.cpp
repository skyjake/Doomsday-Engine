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

DENG2_PIMPL_NOREF(DummyCdChannel)
{
    PlayingMode mode = NotPlaying;
    bool paused      = false;
    dint track       = -1;
    dfloat frequency = 1;
    dfloat volume    = 1;
};

DummyCdChannel::DummyCdChannel() : CdChannel(), d(new Instance)
{}

PlayingMode DummyCdChannel::mode() const
{
    return d->mode;
}

void DummyCdChannel::play(PlayingMode mode)
{
    if(isPlaying()) return;
    if(mode == NotPlaying) return;

    if(d->track < 0) Error("DummyCdChannel::play", "No track is bound");
    d->mode = mode;
}

void DummyCdChannel::stop()
{
    // Nothing to do.
}

bool DummyCdChannel::isPaused() const
{
    return d->paused;
}

void DummyCdChannel::pause()
{
    d->paused = true;
}

void DummyCdChannel::resume()
{
    d->paused = false;
}

Channel &DummyCdChannel::setFrequency(de::dfloat newFrequency)
{
    d->frequency = newFrequency;
    return *this;
}

Channel &DummyCdChannel::setPositioning(Positioning)
{
    // Not supported.
    return *this;
}

Channel &DummyCdChannel::setVolume(de::dfloat newVolume)
{
    d->volume = newVolume;
    return *this;
}

dfloat DummyCdChannel::frequency() const
{
    return d->frequency;
}

Positioning DummyCdChannel::positioning() const
{
    return StereoPositioning;  // Always.
}

dfloat DummyCdChannel::volume() const
{
    return d->volume;
}

void DummyCdChannel::bindTrack(dint track)
{
    d->track = de::max(-1, track);
}

}  // namespace audio
