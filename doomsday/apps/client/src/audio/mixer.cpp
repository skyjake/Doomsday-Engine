/** @file mixer.cpp  Audio channel mixer.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "audio/mixer.h"

#include "audio/system.h"
#include <QMap>
#include <QSet>
#include <QtAlgorithms>

// Debug visual headers:
#include "audio/samplecache.h"
#include "gl/gl_main.h"
#include "api_fontrender.h"
#include "render/rend_font.h"
#include "ui/ui_main.h"
#include "clientapp.h"
#include "def_main.h"           // ::defs
#include <de/concurrency.h>

using namespace de;

namespace audio {

DENG2_PIMPL(Mixer::Track)
, DENG2_OBSERVES(Deletable, Deletion)
{
    Mixer &mixer;              ///< Owner of the track.

    String id;
    String title;

    QSet<Channel *> channels;  ///< All mapped channels (not owned).

    Instance(Public *i, Mixer &mixer)
        : Base(i)
        , mixer(mixer)
    {}

    ~Instance()
    {
        // If any channels are still mapped - forget about them.
        for(Channel *channel : channels.values())
        {
            self.removeChannel(channel);
        }
        DENG2_ASSERT(channels.isEmpty());
    }

    void objectWasDeleted(Deletable *deleted)
    {
        DENG2_ASSERT(channels.contains(reinterpret_cast<Channel *>(deleted)));
        self.removeChannel(reinterpret_cast<Channel *>(deleted));
    }

    DENG2_PIMPL_AUDIENCE(ChannelsRemapped)
};

DENG2_AUDIENCE_METHOD(Mixer::Track, ChannelsRemapped)

Mixer::Track::Track(Mixer &mixer, String const &trackId)
    : d(new Instance(this, mixer))
{
    d->id = trackId;
}

Mixer &Mixer::Track::mixer()
{
    return d->mixer;
}

Mixer const &Mixer::Track::mixer() const
{
    return d->mixer;
}

String Mixer::Track::id() const
{
    return d->id;
}

String Mixer::Track::title() const
{
    return d->title;
}

void Mixer::Track::setTitle(String const &newTitle)
{
    d->title = newTitle;
}

dint Mixer::Track::channelCount() const
{
    return d->channels.count();
}

Mixer::Track &Mixer::Track::addChannel(Channel *channel)
{
    dint const sizeBefore = d->channels.size();
    d->channels.insert(channel);
    if(d->channels.size() != sizeBefore)
    {
        // Start observing the channel so that we can unmap it automatically.
        channel->audienceForDeletion += d;

        // Notify interested parties:
        DENG2_FOR_AUDIENCE2(ChannelsRemapped, i) i->trackChannelsRemapped(*this);
    }
    return *this;
}

Mixer::Track &Mixer::Track::removeChannel(Channel *channel)
{
    dint const sizeBefore = d->channels.size();
    d->channels.remove(channel);
    if(d->channels.size() != sizeBefore)
    {
        channel->audienceForDeletion -= d;

        // Notify interested parties:
        DENG2_FOR_AUDIENCE2(ChannelsRemapped, i) i->trackChannelsRemapped(*this);
    }
    return *this;
}

LoopResult Mixer::Track::forAllChannels(std::function<LoopResult (Channel &)> callback) const
{
    for(Channel *channel : d->channels)
    {
        if(auto result = callback(*channel)) return result;
    }
    return LoopContinue;
}

DENG2_PIMPL_NOREF(Mixer)
{
    struct Tracks : QMap<String /*key: trackId*/, Track *>
    {
        ~Tracks() { DENG2_ASSERT(isEmpty()); }
    } tracks;
};

Mixer::Mixer() : d(new Instance)
{}

Mixer::~Mixer()
{
    clearTracks();
}

void Mixer::clearTracks()
{
    LOG_AS("audio::Mixer");
    qDeleteAll(d->tracks);
    d->tracks.clear();
}

bool Mixer::hasTrack(String const &trackId) const
{
    return !trackId.isEmpty() && d->tracks.contains(trackId.toLower());
}

Mixer::Track &Mixer::findTrack(String const &trackId) const
{
    if(Track *found = tryFindTrack(trackId)) return *found;
    /// @throw MissingTrackError  Unknown trackId specified.
    throw MissingTrackError("Mixer::findTrack", "Unknown track ID \"" + trackId + "\"");
}

Mixer::Track *Mixer::tryFindTrack(de::String const &trackId) const
{
    if(!trackId.isEmpty())
    {
        auto &found = d->tracks.find(trackId.toLower());
        if(found != d->tracks.end()) return found.value();
    }
    return nullptr;
}

LoopResult Mixer::forAllTracks(std::function<LoopResult (Track &)> callback) const
{
    for(Track *track : d->tracks)
    {
        if(auto result = callback(*track)) return result;
    }
    return LoopContinue;
}

dint Mixer::trackCount() const
{
    return d->tracks.count();
}

Mixer::Track &Mixer::makeTrack(String const &trackId, Channel *channel)
{
    DENG2_ASSERT(!trackId.isEmpty());

    LOG_AS("audio::Mixer");
    if(Track *existing = tryFindTrack(trackId))
    {
        existing->addChannel(channel);
        return *existing;
    }

    std::unique_ptr<Track> track(new Track(*this, trackId.toLower()));
    d->tracks.insert(trackId.toLower(), track.get());
    return *track.release();
}

}  // namespace audio

using namespace ::audio;

// Debug visual: -----------------------------------------------------------------

dint showMixerInfo;
//byte refMonitor;

void UI_AudioMixerDrawer()
{
    if(!::showMixerInfo) return;

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
    if(!ClientApp::audioSystem().soundPlaybackAvailable())
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
    ClientApp::audioSystem().sampleCache().info(&cachesize, &ccnt);
    char buf[200]; sprintf(buf, "Cached:%i (%i)", cachesize, ccnt);

    FR_SetColor(1, 1, 1);
    FR_DrawTextXY(buf, 10, 0);

    // Print a line of info about each channel.
    Mixer const &mixer = ClientApp::audioSystem().mixer();
    dint idx = 0;
    mixer["fx"].forAllChannels([&lh, &idx] (Channel &base)
    {
        auto &ch = base.as<SoundChannel>();
        Sound *sound = ch.isPlaying() ? ch.sound() : nullptr;

        FR_SetColor(1, 1, ch.isPlaying() ? 1 : 0);

        char buf[200];
        sprintf(buf, "%02i: %c%c%c v=%3.1f f=%3.3f st=%i et=%u mobj=%i",
                idx,
                !(sound && sound->flags().testFlag(SoundFlag::NoOrigin))            ? 'O' : '.',
                !(sound && sound->flags().testFlag(SoundFlag::NoVolumeAttenuation)) ? 'A' : '.',
                (sound && sound->emitter())                                         ? 'E' : '.',
                ch.volume(), ch.frequency(), ch.startTime(), ch.endTime(),
                (sound && sound->emitter()) ? sound->emitter()->thinker.id : 0);
        FR_DrawTextXY(buf, 5, lh * (1 + idx * 2));

        //if(ch.isValid())
        {
            ::audio::Sound const *sound = ch.sound();
            sfxsample_t const *sample   = sound ? ClientApp::audioSystem().sampleCache().cache(sound->effectId()) : nullptr;

            Block soundDefId;
            if(sound)
            {
                soundDefId = ::defs.sounds[sound->effectId()].gets("id").toUtf8();
            }

            sprintf(buf, "    %c%c%c id=%03i/%-8s ln=%05i b=%i rt=%2i",// bs=%05i (C%05i/W%05i)"
                    ch.isPlaying()                        ? 'P' : '.',
                    ch.mode() == PlayingMode::Looping     ? 'L' : '.',
                    ch.positioning() == StereoPositioning ? 'S' : '3',
                    sound ? sound->effectId() : 0, soundDefId.constData(),
                    sound ? sample->size : 0,
                    ch.bytes(), ch.rate() / 1000);/*,
                    ch.buffer().length, ch.buffer().cursor, ch.buffer().written);*/
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
