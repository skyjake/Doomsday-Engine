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

#include "def_main.h"           // ::defs
#include "sys_system.h"         // Sys_Sleep()

#include "audio/system.h"
#include "audio/samplecache.h"
#include "world/thinkers.h"

#include <de/Log>
#include <de/concurrency.h>
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
        if(!System::get().sfx()) goto noRefresh;

        if(System::get().sfx()->Getv)
        {
            System::get().sfx()->Getv(SFXIP_DISABLE_CHANNEL_REFRESH, &disableRefresh);
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
        while(System::get().sfxIsAvailable() && System::get().hasChannels())
        {
            // The bit is swapped on each refresh (debug info).
            ::refMonitor ^= 1;

            if(!inst.paused)
            {
                // Do the refresh.
                inst.refreshing = true;
                System::get().channels().refreshAll();
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
    QList<Sound/*Channel*/ *> all;

    Instance(Public *i) : Base(i) 
    {
        System::get().sampleCache().audienceForSampleRemove() += this;
    }

    ~Instance()
    {
        clearAll();

        System::get().sampleCache().audienceForSampleRemove() -= this;
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
        self.forAll([&sample] (Sound/*Channel*/ &ch)
        {
            if(ch.hasBuffer())
            {
                sfxbuffer_t &sbuf = ch.buffer();
                if(sbuf.sample && sbuf.sample->soundId == sample.soundId)
                {
                    // Stop and unload.
                    ch.ifs().gen.Reset(&sbuf);
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
    DENG2_ASSERT( System::get().sfxIsAvailable() );  // sanity check

    dint count = 0;
    forAll([&soundId, &count] (Sound/*Channel*/ &ch)
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

Sound/*Channel*/ &Channels::add(Sound &sound)
{
    if(!d->all.contains(&sound))
    {
        /// @todo Log sound configuration, update lookup tables for buffer configs, etc...
        d->all << &sound;
    }
    return sound;
}

Sound/*Channel*/ *Channels::tryFindVacant(bool use3D, dint bytes, dint rate, dint soundId) const
{
    for(Sound/*Channel*/ *ch : d->all)
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

LoopResult Channels::forAll(std::function<LoopResult (Sound/*Channel*/ &)> func) const
{
    for(Sound/*Channel*/ *ch : d->all)
    {
        if(auto result = func(*ch)) return result;
    }
    return LoopContinue;
}

void Channels::refreshAll()
{
    forAll([this] (Sound/*Channel*/ &ch)
    {
        if(ch.hasBuffer() && (ch.buffer().flags & SFXBF_PLAYING))
        {
            ch.ifs().gen.Refresh(&ch.buffer());
        }
        return LoopContinue;
    });
}

void Channels::releaseAllBuffers()
{
    refresher.pause();
    forAll([this] (Sound/*Channel*/ &ch)
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
    if(!audio::System::get().sfxIsAvailable())
    {
        FR_DrawTextXY("Sfx disabled", 0, 0);
        glDisable(GL_TEXTURE_2D);
        return;
    }

    if(::refMonitor)
        FR_DrawTextXY("!", 0, 0);

    // Sample cache information.
    duint cachesize, ccnt;
    audio::System::get().sampleCache().info(&cachesize, &ccnt);
    char buf[200]; sprintf(buf, "Cached:%i (%i)", cachesize, ccnt);

    FR_SetColor(1, 1, 1);
    FR_DrawTextXY(buf, 10, 0);

    // Print a line of info about each channel.
    dint idx = 0;
    audio::System::get().channels().forAll([&lh, &idx] (Sound/*Channel*/ &ch)
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
