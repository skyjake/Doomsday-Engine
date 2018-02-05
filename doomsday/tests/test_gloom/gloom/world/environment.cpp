#include "environment.h"
#include "world.h"
#include "../audio/audiosystem.h"

using namespace de;

DENG2_PIMPL(Environment)
{
    World *world;
    QSet<Sound *> sounds;
    float maxDist;
    TimeSpan sinceLastUpdate;
    bool enabled;

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

void Environment::update(TimeSpan const &elapsed)
{
    d->update(elapsed);
}
