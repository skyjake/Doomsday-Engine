/** @file dummymusicchannel.cpp  Dummy audio::Channel for simulating music playback.
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

#include "audio/dummymusicchannel.h"

#include <de/Error>

using namespace de;

namespace audio {

DENG2_PIMPL_NOREF(DummyMusicChannel)
{
    bool paused       = false;
    PlayingMode mode  = NotPlaying;
    dfloat frequency  = 1;
    dfloat volume     = 1;
    String sourcePath;
};

DummyMusicChannel::DummyMusicChannel() : MusicChannel(), d(new Instance)
{}

PlayingMode DummyMusicChannel::mode() const
{
    return d->mode;
}

void DummyMusicChannel::play(PlayingMode mode)
{
    if(isPlaying()) return;
    if(mode == NotPlaying) return;

    if(d->sourcePath.isEmpty()) Error("DummyMusicChannel::play", "No track is bound");
    d->mode = mode;
}

void DummyMusicChannel::stop()
{
    // Nothing to do.
}

bool DummyMusicChannel::isPaused() const
{
    return d->paused;
}

void DummyMusicChannel::pause()
{
    d->paused = true;
}

void DummyMusicChannel::resume()
{
    d->paused = false;
}

Channel &DummyMusicChannel::setFrequency(dfloat newFrequency)
{
    d->frequency = newFrequency;
    return *this;
}

Channel &DummyMusicChannel::setPositioning(Positioning)
{
    // Not supported.
    return *this;
}

Channel &DummyMusicChannel::setVolume(dfloat newVolume)
{
    d->volume = newVolume;
    return *this;
}

dfloat DummyMusicChannel::frequency() const
{
    return d->frequency;
}

Positioning DummyMusicChannel::positioning() const
{
    return StereoPositioning;  // Always.
}

dfloat DummyMusicChannel::volume() const
{
    return d->volume;
}

bool DummyMusicChannel::canPlayBuffer() const
{
    return false;  /// @todo Should support this...
}

void *DummyMusicChannel::songBuffer(duint)
{
    return nullptr;
}

bool DummyMusicChannel::canPlayFile() const
{
    return true;
}

void DummyMusicChannel::bindFile(String const &sourcePath)
{
    d->sourcePath = sourcePath;
}

}  // namespace audio
