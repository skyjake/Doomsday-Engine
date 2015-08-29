/** @file channel.cpp  Logical sound playback channel.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "audio/channel.h"

#include "audio/samplecache.h"
#include "world/thinkers.h"
#include "dd_main.h"         // remove me
#include "def_main.h"        // ::defs
#include "sys_system.h"      // Sys_Sleep()
#include <de/Log>
#include <de/concurrency.h>
#include <de/timer.h>        // TICSPERSEC
#include <de/vector1.h>      // remove me
#include <QList>
#include <QtAlgorithms>

// Debug visual headers:
//#include "audio/samplecache.h"
#include "gl/gl_main.h"
#include "api_fontrender.h"
#include "render/rend_font.h"
#include "ui/ui_main.h"
//#include <de/concurrency.h>

using namespace de;

namespace audio {

static inline audio::System &system()
{
    return App_AudioSystem();
}

DENG2_PIMPL_NOREF(Channel)
{
    dint flags = 0;                 ///< SFXCF_* flags.
    dfloat frequency = 0;           ///< Frequency adjustment: 1.0 is normal.
    dfloat volume = 0;              ///< Sound volume: 1.0 is max.

    mobj_t *emitter = nullptr;      ///< Mobj emitter for the sound, if any (not owned).
    coord_t origin[3];              ///< Emit from here (synced with emitter).

    sfxbuffer_t *buffer = nullptr;  ///< Assigned sound buffer, if any (not owned).
    dint startTime = 0;             ///< When the assigned sound sample was last started.

    Instance() { zap(origin); }
};

Channel::Channel() : d(new Instance)
{}

Channel::~Channel()
{}

bool Channel::hasBuffer() const
{
    return d->buffer != nullptr;
}

sfxbuffer_t &Channel::buffer()
{
    if(d->buffer) return *d->buffer;
    /// @throw MissingBufferError  No sound buffer is currently assigned.
    throw MissingBufferError("audio::Channel::buffer", "No sound buffer is assigned");
}

sfxbuffer_t const &Channel::buffer() const
{
    return const_cast<Channel *>(this)->buffer();
}

void Channel::setBuffer(sfxbuffer_t *newBuffer)
{
    d->buffer = newBuffer;
}

void Channel::stop()
{
    if(!d->buffer) return;

    system().sfx()->Stop(d->buffer);
}

dint Channel::flags() const
{
    return d->flags;
}

/// @todo Use QFlags -ds
void Channel::setFlags(dint newFlags)
{
    d->flags = newFlags;
}

dfloat Channel::frequency() const
{
    return d->frequency;
}

void Channel::setFrequency(dfloat newFrequency)
{
    d->frequency = newFrequency;
}

dfloat Channel::volume() const
{
    return d->volume;
}

void Channel::setVolume(dfloat newVolume)
{
    d->volume = newVolume;
}

mobj_t *Channel::emitter() const
{
    return d->emitter;
}

void Channel::setEmitter(mobj_t *newEmitter)
{
    d->emitter = newEmitter;
}

void Channel::setFixedOrigin(Vector3d const &newOrigin)
{
    d->origin[0] = newOrigin.x;
    d->origin[1] = newOrigin.y;
    d->origin[2] = newOrigin.z;
}

dfloat Channel::priority() const
{
    if(!d->buffer || !(d->buffer->flags & SFXBF_PLAYING))
        return SFX_LOWEST_PRIORITY;

    if(d->flags & SFXCF_NO_ORIGIN)
        return system().rateSoundPriority(0, 0, d->volume, d->startTime);

    // d->origin is set to emitter->xyz during updates.
    return system().rateSoundPriority(0, d->origin, d->volume, d->startTime);
}

/// @todo audio::System should observe. -ds
void Channel::updatePriority()
{
    // If no sound buffer is assigned we've no need to update.
    sfxbuffer_t *sbuf = d->buffer;
    if(!sbuf) return;

    // Disabled?
    if(d->flags & SFXCF_NO_UPDATE) return;

    // If we know the emitter update our origin info.
    if(mobj_t *emitter = d->emitter)
    {
        d->origin[0] = emitter->origin[0];
        d->origin[1] = emitter->origin[1];
        d->origin[2] = emitter->origin[2];

        // If this is a mobj, center the Z pos.
        if(Thinker_IsMobjFunc(emitter->thinker.function))
        {
            // Sounds originate from the center.
            d->origin[2] += emitter->height / 2;
        }
    }

    // Frequency is common to both 2D and 3D sounds.
    system().sfx()->Set(sbuf, SFXBP_FREQUENCY, d->frequency);

    if(sbuf->flags & SFXBF_3D)
    {
        // Volume is affected only by maxvol.
        system().sfx()->Set(sbuf, SFXBP_VOLUME, d->volume * system().soundVolume() / 255.0f);
        if(d->emitter && d->emitter == system().sfxListener())
        {
            // Emitted by the listener object. Go to relative position mode
            // and set the position to (0,0,0).
            dfloat vec[3]; vec[0] = vec[1] = vec[2] = 0;
            system().sfx()->Set(sbuf, SFXBP_RELATIVE_MODE, true);
            system().sfx()->Setv(sbuf, SFXBP_POSITION, vec);
        }
        else
        {
            // Use the channel's map space origin.
            dfloat origin[3];
            V3f_Copyd(origin, d->origin);
            system().sfx()->Set(sbuf, SFXBP_RELATIVE_MODE, false);
            system().sfx()->Setv(sbuf, SFXBP_POSITION, origin);
        }

        // If the sound is emitted by the listener, speed is zero.
        if(d->emitter && d->emitter != system().sfxListener() &&
           Thinker_IsMobjFunc(d->emitter->thinker.function))
        {
            dfloat vec[3];
            vec[0] = d->emitter->mom[0] * TICSPERSEC;
            vec[1] = d->emitter->mom[1] * TICSPERSEC;
            vec[2] = d->emitter->mom[2] * TICSPERSEC;
            system().sfx()->Setv(sbuf, SFXBP_VELOCITY, vec);
        }
        else
        {
            // Not moving.
            dfloat vec[3]; vec[0] = vec[1] = vec[2] = 0;
            system().sfx()->Setv(sbuf, SFXBP_VELOCITY, vec);
        }
    }
    else
    {
        dfloat dist = 0;
        dfloat pan  = 0;

        // This is a 2D buffer.
        if((d->flags & SFXCF_NO_ORIGIN) ||
           (d->emitter && d->emitter == system().sfxListener()))
        {
            dist = 1;
            pan = 0;
        }
        else
        {
            // Calculate roll-off attenuation. [.125/(.125+x), x=0..1]
            Rangei const &attenRange = system().soundVolumeAttenuationRange();

            dist = Mobj_ApproxPointDistance(system().sfxListener(), d->origin);

            if(dist < attenRange.start || (d->flags & SFXCF_NO_ATTENUATION))
            {
                // No distance attenuation.
                dist = 1;
            }
            else if(dist > attenRange.end)
            {
                // Can't be heard.
                dist = 0;
            }
            else
            {
                dfloat const normdist = (dist - attenRange.start) / attenRange.size();

                // Apply the linear factor so that at max distance there
                // really is silence.
                dist = .125f / (.125f + normdist) * (1 - normdist);
            }

            // And pan, too. Calculate angle from listener to emitter.
            if(mobj_t *listener = system().sfxListener())
            {
                dfloat angle = (M_PointToAngle2(listener->origin, d->origin) - listener->angle) / (dfloat) ANGLE_MAX * 360;

                // We want a signed angle.
                if(angle > 180)
                    angle -= 360;

                // Front half.
                if(angle <= 90 && angle >= -90)
                {
                    pan = -angle / 90;
                }
                else
                {
                    // Back half.
                    pan = (angle + (angle > 0 ? -180 : 180)) / 90;
                    // Dampen sounds coming from behind.
                    dist *= (1 + (pan > 0 ? pan : -pan)) / 2;
                }
            }
            else
            {
                // No listener mobj? Can't pan, then.
                pan = 0;
            }
        }

        system().sfx()->Set(sbuf, SFXBP_VOLUME, d->volume * dist * system().soundVolume() / 255.0f);
        system().sfx()->Set(sbuf, SFXBP_PAN, pan);
    }
}

dint Channel::startTime() const
{
    return d->startTime;
}

void Channel::setStartTime(dint newStartTime)
{
    d->startTime = newStartTime;
}

void Channel::releaseBuffer()
{
    stop();
    if(!hasBuffer()) return;

    system().sfx()->Destroy(&buffer());
    setBuffer(nullptr);
}

struct ChannelRefresher
{
    thread_t thread = nullptr;
    volatile bool paused = false;
    volatile bool refreshing = false;

    void pause()
    {
        if(paused) return;  // No change.

        paused = true;
        // Make sure that if currently running, we don't continue until it has stopped.
        while(refreshing)
        {
            Sys_Sleep(0);
        }
        // Sys_SuspendThread(refreshHandle, true);
    }

    void resume()
    {
        if(!paused) return;  // No change.
        paused = false;
        // Sys_SuspendThread(refreshHandle, false);
    }

    /**
     * Start the sound channel refresh thread. It will stop on its own when it
     * notices that the rest of the sound system is going down.
     */
    void init()
    {
        refreshing = false;
        paused     = false;

        dint disableRefresh = false;

        // Nothing to refresh?
        if(!system().sfx()) goto noRefresh;

        if(system().sfx()->Getv)
        {
            system().sfx()->Getv(SFXIP_DISABLE_CHANNEL_REFRESH, &disableRefresh);
        }

        if(!disableRefresh)
        {
            // Start the refresh thread. It will run until the Sfx module is shut down.
            thread = Sys_StartThread(refreshThread, this);
            if(!thread)
            {
                throw Error("audio::ChannelRefresher::init", "Failed starting the refresh thread");
            }
        }
        else
        {
    noRefresh:
            LOGDEV_AUDIO_NOTE("Audio driver does not require a refresh thread");
        }
    }

private:

    /**
     * This is a high-priority thread that periodically checks if the channels need
     * to be updated with more data. The thread terminates when it notices that the
     * channels have been destroyed. The Sfx audio driver maintains a 250ms buffer
     * for each channel, which means the refresh must be done often enough to keep
     * them filled.
     *
     * @todo Use a real mutex, will you?
     */
    static dint C_DECL refreshThread(void *refresher)
    {
        auto &inst = *static_cast<ChannelRefresher *>(refresher);

        // We'll continue looping until the Sfx module is shut down.
        while(system().sfxIsAvailable() && system().hasChannels())
        {
            // The bit is swapped on each refresh (debug info).
            ::refMonitor ^= 1;

            if(!inst.paused)
            {
                // Do the refresh.
                inst.refreshing = true;
                system().channels().refreshAll();
                inst.refreshing = false;

                // Let's take a nap.
                Sys_Sleep(200);
            }
            else
            {
                // Refreshing is not allowed, so take a shorter nap while
                // waiting for allowRefresh.
                Sys_Sleep(150);
            }
        }

        // Time to end this thread.
        return 0;
    }
};

static ChannelRefresher refresher;

DENG2_PIMPL(Channels)
, DENG2_OBSERVES(SampleCache, SampleRemove)
{
    QList<Channel *> all;

    Instance(Public *i) : Base(i) 
    {
        system().sampleCache().audienceForSampleRemove() += this;
    }

    ~Instance()
    {
        clearAll();

        system().sampleCache().audienceForSampleRemove() -= this;
    }

    void clearAll()
    {
        qDeleteAll(all);
        all.clear();
    }

    /**
     * The given @a sample will soon no longer exist. All channels currently loaded
     * with it must be reset.
     */
    void sfxSampleCacheAboutToRemove(sfxsample_t const &sample)
    {
        refresher.pause();
        self.forAll([&sample] (Channel &ch)
        {
            if(ch.hasBuffer())
            {
                sfxbuffer_t &sbuf = ch.buffer();
                if(sbuf.sample && sbuf.sample->soundId == sample.soundId)
                {
                    // Stop and unload.
                    system().sfx()->Reset(&sbuf);
                }
            }
            return LoopContinue;
        });
        refresher.resume();
    }
};

Channels::Channels() : d(new Instance(this))
{}

Channels::~Channels()
{
    // Stop further refreshing if in progress.
    refresher.paused = true;
    if(refresher.thread)
    {
        // Wait for the refresh thread to stop.
        Sys_WaitThread(refresher.thread, 2000, nullptr);
        refresher.thread = nullptr;
    }

    releaseAllBuffers();
}

dint Channels::count() const
{
    return d->all.count();
}

dint Channels::countPlaying(dint soundId)
{
    DENG2_ASSERT( system().sfxIsAvailable() );  // sanity check

    dint count = 0;
    forAll([&soundId, &count] (Channel &ch)
    {
        if(ch.hasBuffer())
        {
            sfxbuffer_t &sbuf = ch.buffer();
            if((sbuf.flags & SFXBF_PLAYING) && sbuf.sample && sbuf.sample->soundId == soundId)
            {
                count += 1;
            }
        }
        return LoopContinue;
    });
    return count;
}

Channel &Channels::add(Channel &newChannel)
{
    if(!d->all.contains(&newChannel))
    {
        /// @todo Log channel configuration, update lookup tables for buffer configs, etc...
        d->all << &newChannel;
    }
    return newChannel;
}

Channel *Channels::tryFindVacant(bool use3D, dint bytes, dint rate, dint soundId) const
{
    for(Channel *ch : d->all)
    {
        if(!ch->hasBuffer()) continue;
        sfxbuffer_t const &sbuf = ch->buffer();

        if((sbuf.flags & SFXBF_PLAYING)
           || use3D != ((sbuf.flags & SFXBF_3D) != 0)
           || sbuf.bytes != bytes
           || sbuf.rate  != rate)
            continue;

        // What about the sample?
        if(soundId > 0)
        {
            if(!sbuf.sample || sbuf.sample->soundId != soundId)
                continue;
        }
        else if(soundId == 0)
        {
            // We're trying to find a channel with no sample already loaded.
            if(sbuf.sample)
                continue;
        }

        // This is perfect, take this!
        return ch;
    }

    return nullptr;  // None suitable.
}

LoopResult Channels::forAll(std::function<LoopResult (Channel &)> func) const
{
    for(Channel *ch : d->all)
    {
        if(auto result = func(*ch)) return result;
    }
    return LoopContinue;
}

void Channels::refreshAll()
{
    forAll([this] (Channel &ch)
    {
        if(ch.hasBuffer() && (ch.buffer().flags & SFXBF_PLAYING))
        {
            system().sfx()->Refresh(&ch.buffer());
        }
        return LoopContinue;
    });
}

void Channels::releaseAllBuffers()
{
    refresher.pause();
    forAll([this] (Channel &ch)
    {
        ch.releaseBuffer();
        return LoopContinue;
    });
    refresher.resume();
}

void Channels::allowRefresh(bool allow)
{
    if(allow)
    {
        refresher.resume();
    }
    else
    {
        refresher.pause();
    }
}

void Channels::initRefresh()
{
    refresher.init();
}

}  // namespace audio

using namespace audio;

// Debug visual: -----------------------------------------------------------------

dint showSoundInfo;
byte refMonitor;

void UI_AudioChannelDrawer()
{
    if(!::showSoundInfo) return;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT, 0, -1, 1);

    glEnable(GL_TEXTURE_2D);

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(1, 1, 0, 1);

    dint const lh = FR_SingleLineHeight("Q");
    if(!App_AudioSystem().sfxIsAvailable())
    {
        FR_DrawTextXY("Sfx disabled", 0, 0);
        glDisable(GL_TEXTURE_2D);
        return;
    }

    if(::refMonitor)
        FR_DrawTextXY("!", 0, 0);

    // Sample cache information.
    duint cachesize, ccnt;
    App_AudioSystem().sampleCache().info(&cachesize, &ccnt);
    char buf[200]; sprintf(buf, "Cached:%i (%i)", cachesize, ccnt);

    FR_SetColor(1, 1, 1);
    FR_DrawTextXY(buf, 10, 0);

    // Print a line of info about each channel.
    dint idx = 0;
    App_AudioSystem().channels().forAll([&lh, &idx] (audio::Channel &ch)
    {
        if(ch.hasBuffer() && (ch.buffer().flags & SFXBF_PLAYING))
        {
            FR_SetColor(1, 1, 1);
        }
        else
        {
            FR_SetColor(1, 1, 0);
        }

        char buf[200];
        sprintf(buf, "%02i: %c%c%c v=%3.1f f=%3.3f st=%i et=%u mobj=%i",
                idx,
                !(ch.flags() & SFXCF_NO_ORIGIN     ) ? 'O' : '.',
                !(ch.flags() & SFXCF_NO_ATTENUATION) ? 'A' : '.',
                ch.emitter() ? 'E' : '.',
                ch.volume(), ch.frequency(), ch.startTime(),
                ch.hasBuffer() ? ch.buffer().endTime : 0,
                ch.emitter()   ? ch.emitter()->thinker.id : 0);
        FR_DrawTextXY(buf, 5, lh * (1 + idx * 2));

        if(ch.hasBuffer())
        {
            sfxbuffer_t &sbuf = ch.buffer();

            sprintf(buf, "    %c%c%c%c id=%03i/%-8s ln=%05i b=%i rt=%2i bs=%05i (C%05i/W%05i)",
                    (sbuf.flags & SFXBF_3D     ) ? '3' : '.',
                    (sbuf.flags & SFXBF_PLAYING) ? 'P' : '.',
                    (sbuf.flags & SFXBF_REPEAT ) ? 'R' : '.',
                    (sbuf.flags & SFXBF_RELOAD ) ? 'L' : '.',
                    sbuf.sample ? sbuf.sample->soundId : 0,
                    sbuf.sample ? ::defs.sounds[sbuf.sample->soundId].gets("id") : "",
                    sbuf.sample ? sbuf.sample->size : 0,
                    sbuf.bytes, sbuf.rate / 1000, sbuf.length,
                    sbuf.cursor, sbuf.written);
            FR_DrawTextXY(buf, 5, lh * (2 + idx * 2));
        }

        idx += 1;
        return LoopContinue;
    });

    glDisable(GL_TEXTURE_2D);

    // Back to the original.
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}
