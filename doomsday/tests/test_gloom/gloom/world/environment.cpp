/** @file environment.cpp
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "gloom/world/environment.h"
#include "gloom/world/world.h"
#include "gloom/audio/audiosystem.h"

using namespace de;

namespace gloom {

DENG2_PIMPL(Environment)
{
    World *       world;
    QSet<Sound *> sounds;
    float         maxDist;
    TimeSpan      sinceLastUpdate;
    bool          enabled;

    Impl(Public *i)
        : Base(i)
        , world(0)
        , maxDist(150)
        , enabled(true)
    {}

    void stopAllSounds()
    {
        foreach(Sound *snd, sounds)
        {
            sounds.remove(snd);
            delete snd;
        }
    }

    void killDistantSounds()
    {
        if (const auto *cam = AudioSystem::get().listener())
        {
            Vector3f const pos = cam->cameraPosition();
            foreach (Sound *snd, sounds)
            {
                if ((pos - snd->position()).length() > maxDist)
                {
                    sounds.remove(snd);
                    delete snd;
                }
            }
        }
    }

    void startNewSound()
    {
        const auto *cam = AudioSystem::get().listener();
        if (!cam) return;

        // Choose a position.
        Vector3f pos = cam->cameraPosition() + Matrix4f::rotate(qrand() % 360, Vector3f(0, 1, 0)) *
                                                   Vector3f(sounds.isEmpty() ? 5 : 30, 0, 0);

        pos.y = world->groundSurfaceHeight(pos) - 3;

        String name;
        float  vol = 1;
        if (pos.y < -5)
        {
            name = "mountain.wind";
            vol  = .3f;
        }
        else if (pos.y > 5)
        {
            name = (qrand() % 2 ? "field.birds" : "field.crickets");
        }

        if (name.isEmpty()) return;

        Sound &snd = AudioSystem::get()
                         .newSound(name)
                         .setPosition(pos)
                         .setMinDistance(15)
                         .setSpatialSpread(45)
                         .setVolume(vol);
        sounds.insert(&snd);
        snd.play(Sound::Looping);

        //qDebug() << "started" << name << "at" << pos.asText();
    }

    void update(TimeSpan const &elapsed)
    {
        if (!enabled) return;

        // We'll update sounds once per second.
        sinceLastUpdate += elapsed;
        if (sinceLastUpdate >= 1.0)
        {
            sinceLastUpdate -= 1;

            killDistantSounds();
            if (sounds.size() < 3)
            {
                startNewSound();
            }
        }
    }
};

Environment::Environment() : d(new Impl(this))
{}

void Environment::enable(bool enabled)
{
    d->enabled = enabled;

    if (!enabled)
    {
        d->stopAllSounds();
    }
    else
    {
        d->sinceLastUpdate = 0;
    }
}

void Environment::setWorld(World *world)
{
    d->world = world;
}

void Environment::advanceTime(TimeSpan const &elapsed)
{
    d->update(elapsed);
}

} // namespace gloom
