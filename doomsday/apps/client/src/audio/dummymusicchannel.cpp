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

DummyMusicChannel::DummyMusicChannel() : MusicChannel()
{}

PlayingMode DummyMusicChannel::mode() const
{
    return _mode;
}

void DummyMusicChannel::play(PlayingMode mode)
{
    if(isPlaying()) return;
    if(mode == NotPlaying) return;

    if(_sourcePath.isEmpty()) Error("DummyMusicChannel::play", "No track is bound");
    _mode = mode;
}

void DummyMusicChannel::stop()
{}

bool DummyMusicChannel::isPaused() const
{
    return _paused;
}

void DummyMusicChannel::pause()
{
    _paused = true;
}

void DummyMusicChannel::resume()
{
    _paused = false;
}

Channel &DummyMusicChannel::setFrequency(de::dfloat newFrequency)
{
    _frequency = newFrequency;
    return *this;
}

Channel &DummyMusicChannel::setPositioning(Positioning)
{
    // Not supported.
    return *this;
}

Channel &DummyMusicChannel::setVolume(de::dfloat newVolume)
{
    _volume = newVolume;
    return *this;
}

dfloat DummyMusicChannel::frequency() const
{
    return _frequency;
}

Positioning DummyMusicChannel::positioning() const
{
    return StereoPositioning;  // Always.
}

dfloat DummyMusicChannel::volume() const
{
    return _volume;
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
    _sourcePath = sourcePath;
}

}  // namespace audio
