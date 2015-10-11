/** @file channel.cpp  Logical sound playback channel.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "audio/system.h"
#include <de/timer.h>
#include <QList>

// Debug visual headers:
#include "audio/samplecache.h"
#include "gl/gl_main.h"
#include "api_fontrender.h"
#include "render/rend_font.h"
#include "ui/ui_main.h"
#include "def_main.h"           // ::defs
#include <de/concurrency.h>

using namespace de;

namespace audio {

DENG2_PIMPL(Channels)
{
    QList<Sound/*Channel*/ *> all;

    Instance(Public *i) : Base(i) 
    {}

    ~Instance()
    {
        clearAll();
    }

    void clearAll()
    {
        all.clear();
        notifyRemapped();
    }

    void notifyRemapped()
    {
        DENG2_FOR_PUBLIC_AUDIENCE2(Remapped, i)
        {
            i->channelsRemapped(self);
        }
    }

    DENG2_PIMPL_AUDIENCE(Remapped)
};

DENG2_AUDIENCE_METHOD(Channels, Remapped)

Channels::Channels() : d(new Instance(this))
{}

dint Channels::count() const
{
    return d->all.count();
}

dint Channels::countPlaying(dint soundId, SoundEmitter *emitter) const
{
    dint count = 0;
    for(Sound/*Channel*/ *ch : d->all)
    {
        if(!ch->isPlaying()) continue;
        if(emitter && ch->emitter() != emitter) continue;
        if(soundId && ch->samplePtr()->soundId != soundId) continue;

        // Once playing, repeating sounds don't stop.
        /*if(!(ch->buffer().flags & SFXBF_REPEAT))
        {
            // Check time. The flag is updated after a slight delay (only at refresh).
            dint const ticsToDelay = ch->samplePtr()->numSamples / dfloat( ch->buffer().freq ) * TICSPERSEC;
            if(Timer_Ticks() - ch->startTime() < ticsToDelay)
                continue;
        }*/
 
        count += 1;
    }
    return count;
}

Sound/*Channel*/ &Channels::add(Sound &sound)
{
    LOG_AS("audio::Channels");
    if(!d->all.contains(&sound))
    {
        /// @todo Log sound configuration, update lookup tables for buffer configs, etc...
        d->all << &sound;

        d->notifyRemapped();
    }
    return sound;
}

dint Channels::stopGroup(dint group, SoundEmitter *emitter)
{
    LOG_AS("audio::Channels");
    dint stopCount = 0;
    for(Sound/*Channel*/ *ch : d->all)
    {
        if(!ch->isPlaying()) continue;

        if(   ch->samplePtr()->group == group
           && (!emitter || ch->emitter() == emitter))
        {
            // This channel must be stopped!
            ch->stop();
            stopCount += 1;
        }
    }

    return stopCount;
}

dint Channels::stopWithEmitter(SoundEmitter *emitter, bool clearSoundEmitter)
{
    LOG_AS("audio::Channels");
    dint stopCount = 0;
    for(Sound/*Channel*/ *ch : d->all)
    {
        if(!ch->emitter()) continue;

        if(!emitter || ch->emitter() == emitter)
        {
            // This channel must be stopped!.
            ch->stop();
            stopCount += 1;

            if(clearSoundEmitter)
            {
                ch->setEmitter(nullptr);
            }
        }
    }
    return stopCount;
}

dint Channels::stopWithLowerPriority(dint soundId, SoundEmitter *emitter, dint defPriority)
{
    LOG_AS("audio::Channels");
    dint stopCount = 0;
    for(Sound/*Channel*/ *ch : d->all)
    {
        if(!ch->isPlaying()) continue;
            
        if(   (soundId && ch->samplePtr()->soundId != soundId)
           || (emitter && ch->emitter() != emitter))
        {
            continue;
        }

        // Can it be stopped?
        if(ch->buffer().flags & SFXBF_DONT_STOP)
        {
            // The emitter might get destroyed...
            ch->setEmitter(nullptr);
            ch->setFlags(ch->flags() | (SFXCF_NO_UPDATE | SFXCF_NO_ORIGIN));
            continue;
        }

        // Check the priority.
        if(defPriority >= 0)
        {
            dint oldPrio = ::defs.sounds[ch->samplePtr()->soundId].geti("priority");
            if(oldPrio < defPriority)  // Old is more important.
            {
                stopCount = -1;
                break;
            }
        }

        // This channel must be stopped!
        ch->stop();
        stopCount += 1;
    }

    return stopCount;
}

Sound/*Channel*/ *Channels::tryFindVacant(bool stereoPositioning, dint bytes, dint rate,
    dint soundId) const
{
    LOG_AS("audio::Channels");
    for(Sound/*Channel*/ *ch : d->all)
    {
        if(!ch->isPlaying()) continue;

        if(   stereoPositioning != ((ch->buffer().flags & SFXBF_3D) == 0)
           || ch->buffer().bytes != bytes
           || ch->buffer().rate  != rate)
            continue;

        // What about the sample?
        if(soundId > 0)
        {
            if(!ch->samplePtr() || ch->samplePtr()->soundId != soundId)
                continue;
        }
        else if(soundId == 0)
        {
            // We're trying to find a channel with no sample already loaded.
            if(ch->samplePtr())
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

}  // namespace audio

using namespace audio;

// Debug visual: -----------------------------------------------------------------

dint showSoundInfo;
//byte refMonitor;

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
    if(!audio::System::get().soundPlaybackAvailable())
    {
        FR_DrawTextXY("Sfx disabled", 0, 0);
        glDisable(GL_TEXTURE_2D);
        return;
    }

    /*
    if(::refMonitor)
        FR_DrawTextXY("!", 0, 0);
    */

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
        if(ch.isPlaying())
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
                ch.volume(), ch.frequency(), ch.startTime(), ch.endTime(),
                ch.emitter() ? ch.emitter()->thinker.id : 0);
        FR_DrawTextXY(buf, 5, lh * (1 + idx * 2));

        if(ch.isValid())
        {
            sfxsample_t const *sample = ch.samplePtr();

            sprintf(buf, "    %c%c%c%c id=%03i/%-8s ln=%05i b=%i rt=%2i bs=%05i (C%05i/W%05i)",
                    (ch.buffer().flags & SFXBF_3D     ) ? '3' : '.',
                    (ch.buffer().flags & SFXBF_PLAYING) ? 'P' : '.',
                    (ch.buffer().flags & SFXBF_REPEAT ) ? 'R' : '.',
                    (ch.buffer().flags & SFXBF_RELOAD ) ? 'L' : '.',
                    sample ? sample->soundId : 0,
                    sample ? ::defs.sounds[sample->soundId].gets("id") : "",
                    sample ? sample->size : 0,
                    ch.buffer().bytes, ch.buffer().rate / 1000, ch.buffer().length,
                    ch.buffer().cursor, ch.buffer().written);
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
