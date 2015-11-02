/** @file stage.cpp  Logical audio context or "soundstage".
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "audio/stage.h"

#include "audio/listener.h"

#include "world/p_object.h"
#include <de/Log>
#include <de/timer.h>
#include <QMultiHash>
#include <QMutableHashIterator>

using namespace de;

namespace audio {

static duint const SOUND_PURGE_INTERVAL = 2000;  ///< 2 seconds

DENG2_PIMPL_NOREF(Stage)
{
    Exclusion exclusion { Exclusion::DontExclude };

    // A "listener" is the "ears" of the user within the soundstage.
    Listener listener;

    /**
     * (Logical) Sounds track currently-playing sounds somewhere in the stage on a purely
     * logical level (irrespective of whether playback channels are available, or if the
     * sound will actually be audible to anyone).
     */
    struct SoundHash : public QMultiHash<dint /*key: soundId*/, Sound>
    {
        typedef QMutableHashIterator<dint /*key: soundId*/, Sound> MutableIterator;

        ~SoundHash() { DENG2_ASSERT(isEmpty()); }  // Should be empty by now.
    } sounds;

    duint lastSoundPurge = 0;
};

Stage::Stage(Exclusion exclusion) : d(new Instance)
{
    setExclusion(exclusion);
}

Stage::~Stage()
{
    removeAllSounds();
}

Stage::Exclusion Stage::exclusion() const
{
    return d->exclusion;
}

void Stage::setExclusion(Exclusion newBehavior)
{
    d->exclusion = newBehavior;
}

Listener &Stage::listener()
{
    return d->listener;
}

Listener const &Stage::listener() const
{
    return d->listener;
}

bool Stage::soundIsPlaying(dint soundId, SoundEmitter *emitter) const
{
    //LOG_AS("audio::Stage");
    duint const nowTime = Timer_RealMilliseconds();
    auto it = soundId > 0 ? d->sounds.constFind(soundId) : d->sounds.constBegin();
    while(it != d->sounds.constEnd())
    {
        if(soundId > 0 && it.key() != soundId)
            break;

        Sound const &sound = it.value();
        if(sound.emitter() == emitter && sound.isPlaying(nowTime))
            return true;

        ++it;
    }
    return false;  // Not playing.
}

void Stage::addSound(Sound const &sound)
{
    // Reject sounds with an invalid sound ID.
    DENG2_ASSERT(sound.soundId() > 0);
    if(sound.soundId() <= 0) return;

    LOG_AS("audio::Stage");

    // Only one Sound per SoundEmitter?
    if(sound.emitter() && exclusion() == OnePerEmitter)
    {
        // Remove all existing (logical) Sounds emitted by it, from the sound stage.
        // Playback is stopped a little later...
        Instance::SoundHash::MutableIterator it(d->sounds);
        while(it.hasNext())
        {
            it.next();
            if(it.value().emitter() != sound.emitter())
                continue;

            it.remove();
        }
    }

    d->sounds.insert(sound.soundId(), sound);  // A copy is made.
}

void Stage::removeAllSounds()
{
    //LOG_AS("audio::Stage");
    d->sounds.clear();
}

void Stage::removeSoundsById(dint soundId)
{
    //LOG_AS("audio::Stage");
    d->sounds.remove(soundId);
}

void Stage::removeSoundsWithEmitter(SoundEmitter const &emitter)
{
    //LOG_AS("audio::Stage");
    Instance::SoundHash::MutableIterator it(d->sounds);
    while(it.hasNext())
    {
        it.next();
        if(it.value().emitter() != &emitter) continue;

        it.remove();
    }
}

void Stage::maybeRunSoundPurge()
{
    // Too soon?
    duint const nowTime = Timer_RealMilliseconds();
    if(nowTime - d->lastSoundPurge < SOUND_PURGE_INTERVAL)
        return;

    //LOG_AS("audio::Stage");
    //LOGDEV_AUDIO_XVERBOSE("Purging logical sounds...");

    // Check all sounds in the hash.
    Instance::SoundHash::MutableIterator it(d->sounds);
    while(it.hasNext())
    {
        it.next();
        if(!it.value().isPlaying(nowTime))
        {
            // This has stopped.
            it.remove();
        }
    }

    // Purge completed.
    d->lastSoundPurge = nowTime;
}

}  // namespace audio
